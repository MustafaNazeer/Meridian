// Integration test: ITCH replay diff between the C++ matching engine
// (via meridian-replay) and the Python reference (via tests/reference/
// itch_replay.py). Both sides parse the same tape, both sides drive
// their respective engine, both sides emit JSON Lines audit output.
// The two outputs must match byte for byte.
//
// The fixture tape is generated at test time via meridian-tape-gen so
// the repo carries no binary blob and the input bytes are derivable
// from a (seed, message_count, cancel_pct) tuple. The tape, both
// audit outputs, and the working directory all live under a per-test
// /tmp directory that is removed at the end.
//
// Acceptance: the comparison passes byte for byte at the documented
// tape size (10000 messages, seed 42, 30 percent cancels). The lower
// bound on the diff payload size guards against a silent "both sides
// emitted nothing" pass.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

// Run a shell command and return its exit code. Used for the three
// external steps (tape gen, C++ replay, Python replay) so each
// step's stderr surfaces in the test log on failure.
int run(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) lines.push_back(line);
    return lines;
}

class ItchReplayDiff : public ::testing::Test {
protected:
    void SetUp() override {
        char dir[] = "/tmp/meridian_itch_diff_XXXXXX";
        ASSERT_NE(mkdtemp(dir), nullptr);
        workdir_ = dir;
    }

    void TearDown() override {
        if (!workdir_.empty()) {
            const std::string cmd = "rm -rf " + workdir_;
            [[maybe_unused]] int rc = std::system(cmd.c_str());
        }
    }

    std::string workdir_;
};

TEST_F(ItchReplayDiff, AddAndDeleteTapeMatchesReference) {
    const std::string tape  = workdir_ + "/tape.bin";
    const std::string cpp_o = workdir_ + "/cpp.jsonl";
    const std::string py_o  = workdir_ + "/py.jsonl";

    // Build the fixture tape: 10000 messages, seed 42, 30 percent
    // cancels. Smaller than the bench's 1M but enough to exercise the
    // matching loop on dozens of crossings and cancels.
    const std::string gen_cmd =
        std::string(MERIDIAN_TAPE_GEN_PATH) +
        " --out " + tape +
        " --messages 10000 --seed 42 --cancel-pct 30 2>/dev/null";
    ASSERT_EQ(run(gen_cmd), 0) << "tape generator failed";

    // C++ replay.
    const std::string cpp_cmd =
        std::string(MERIDIAN_REPLAY_PATH) +
        " --tape " + tape + " --audit " + cpp_o + " 2>/dev/null";
    ASSERT_EQ(run(cpp_cmd), 0) << "meridian-replay failed";

    // Python reference replay.
    const std::string py_cmd =
        "PYTHONPATH=" SOURCE_REFERENCE_DIR
        " python3 " SOURCE_REFERENCE_DIR "/itch_replay.py"
        " --tape " + tape + " > " + py_o + " 2>/dev/null";
    ASSERT_EQ(run(py_cmd), 0) << "Python itch_replay failed";

    auto cpp_lines = read_lines(cpp_o);
    auto py_lines  = read_lines(py_o);

    // Sanity floor: the 10000-message fixture produces several thousand
    // reports. A trivial pass (both sides empty) would be a bug.
    ASSERT_GT(cpp_lines.size(), 1000u)
        << "C++ replay produced " << cpp_lines.size()
        << " reports; expected several thousand. Suspect a parse / dispatch bug.";

    ASSERT_EQ(cpp_lines.size(), py_lines.size())
        << "C++ produced " << cpp_lines.size() << " reports, "
        << "Python produced " << py_lines.size();
    for (std::size_t i = 0; i < cpp_lines.size(); ++i) {
        ASSERT_EQ(cpp_lines[i], py_lines[i])
            << "report " << i << " disagrees";
    }
}

}  // namespace
