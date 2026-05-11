type Props = {
  title: string;
  aux: string;
};

export function SectionHead({ title, aux }: Props) {
  return (
    <div
      data-component="SectionHead"
      className="flex items-baseline justify-between border-b border-rule pb-3 mb-5.5"
    >
      <h2
        data-element="SectionTitle"
        className="font-display italic font-medium text-section text-ink tracking-tight-section"
      >
        {title}
      </h2>
      <span
        data-element="SectionAux"
        className="font-body font-medium uppercase text-micro text-gold tracking-section-aux"
      >
        {aux}
      </span>
    </div>
  );
}
