import type { Config } from "tailwindcss";

const config: Config = {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  darkMode: "class",
  theme: {
    screens: {
      sm: "640px",
      md: "768px",
      lg: "1024px",
      xl: "1280px",
      "2xl": "1536px",
    },
    extend: {
      colors: {
        bg: {
          DEFAULT: "#0E1126",
          deep: "#0A0D1F",
          radial: "#1A1E40",
        },
        surface: {
          DEFAULT: "#161A33",
          raised: "#1B1F3D",
          overlay: "rgba(22, 26, 51, 0.85)",
        },
        rule: {
          DEFAULT: "#232847",
          strong: "#2F365A",
          tape: "rgba(35, 40, 71, 0.5)",
        },
        ink: {
          DEFAULT: "#EFE9DC",
          2: "#BFB7A8",
          soft: "#8C879A",
          faint: "#56536A",
        },
        gold: {
          DEFAULT: "#D4A24C",
          deep: "#B97D2C",
        },
        bid: {
          DEFAULT: "#4FA89E",
          soft: "rgba(79, 168, 158, 0.16)",
        },
        ask: {
          DEFAULT: "#C5765B",
          soft: "rgba(197, 118, 91, 0.16)",
        },
      },
      fontFamily: {
        display: ["Newsreader", "ui-serif", "Georgia", "serif"],
        body: ["Inter", "ui-sans-serif", "system-ui", "sans-serif"],
        mono: [
          "JetBrains Mono",
          "ui-monospace",
          "SFMono-Regular",
          "Menlo",
          "monospace",
        ],
      },
      fontSize: {
        micro: ["10px", { lineHeight: "1.4" }],
        meta: ["11px", { lineHeight: "1.4" }],
        trades: ["12px", { lineHeight: "1.5" }],
        ladder: ["13px", { lineHeight: "1.5" }],
        body: ["14px", { lineHeight: "1.5" }],
        "perf-cell": ["16px", { lineHeight: "1.2" }],
        section: ["22px", { lineHeight: "1.2", letterSpacing: "-0.01em" }],
        "display-stat": ["32px", { lineHeight: "1", letterSpacing: "-0.02em" }],
        "display-sm": ["38px", { lineHeight: "1", letterSpacing: "-0.02em" }],
        mark: ["42px", { lineHeight: "1", letterSpacing: "-0.015em" }],
        "display-xl": ["72px", { lineHeight: "1", letterSpacing: "-0.02em" }],
      },
      letterSpacing: {
        "tight-display": "-0.02em",
        "tight-mark": "-0.015em",
        "tight-section": "-0.01em",
        stat: "0.04em",
        "perf-label": "0.08em",
        aux: "0.12em",
        "section-aux": "0.14em",
      },
      lineHeight: {
        replay: "2",
      },
      spacing: {
        "0.5": "2px",
        "0.75": "3px",
        "1.5": "6px",
        "2.5": "10px",
        "3.5": "14px",
        "4.5": "18px",
        "5.5": "22px",
        "11": "44px",
        "14": "56px",
      },
      borderRadius: {
        none: "0",
        bar: "1px",
        full: "9999px",
      },
      boxShadow: {
        none: "none",
        "glow-gold": "0 0 10px #D4A24C",
        "glow-bid": "0 0 8px #4FA89E",
        "glow-ask": "0 0 8px #C5765B",
      },
      backdropBlur: {
        chip: "8px",
      },
      backgroundImage: {
        "body-radial":
          "radial-gradient(ellipse at top, #1A1E40 0%, #0E1126 55%, #0A0D1F 100%)",
        "spread-band":
          "linear-gradient(to right, transparent, rgba(212, 162, 76, 0.12) 20%, rgba(212, 162, 76, 0.12) 80%, transparent)",
        histogram: "linear-gradient(to top, #B97D2C, #D4A24C)",
        "replay-progress": "linear-gradient(to right, #B97D2C, #D4A24C)",
      },
      keyframes: {
        "pulse-live": {
          "50%": { opacity: "0.4" },
        },
      },
      animation: {
        "pulse-live": "pulse-live 1.6s infinite",
      },
      gridTemplateColumns: {
        hero: "2fr 1fr 1fr 1fr",
        main: "380px 1fr 320px",
        perf: "1fr 1fr",
      },
    },
  },
  corePlugins: {
    boxShadow: true,
  },
  plugins: [
    function ({ addBase, addUtilities }: any) {
      addBase({
        "html, body": { height: "100%" },
        body: {
          fontFamily: "Inter, system-ui, sans-serif",
          backgroundColor: "#0E1126",
          backgroundImage:
            "radial-gradient(ellipse at top, #1A1E40 0%, #0E1126 55%, #0A0D1F 100%)",
          backgroundAttachment: "fixed",
          color: "#EFE9DC",
          fontSize: "14px",
          lineHeight: "1.5",
          fontFeatureSettings: "'tnum', 'cv11', 'ss01'",
          WebkitFontSmoothing: "antialiased",
        },
      });
      addUtilities({
        ".num": {
          fontFamily: "'JetBrains Mono', ui-monospace, monospace",
          fontVariantNumeric: "tabular-nums",
        },
        ".editorial": {
          fontFamily: "Newsreader, ui-serif, serif",
          fontStyle: "italic",
          fontWeight: "400",
        },
      });
    },
  ],
};

export default config;
