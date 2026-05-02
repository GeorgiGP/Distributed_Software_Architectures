#!/usr/bin/env python3
from pypdf import PdfReader, PdfWriter

def main():
    source_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_0MI0600299_v3.pdf"
    section_6_3 = "/Users/I764233/uni/RSA/docs/section_6_3_wide.pdf"
    section_7_6 = "/Users/I764233/uni/RSA/docs/section_7_6_wide.pdf"
    section_8 = "/Users/I764233/uni/RSA/docs/section_8_updated.pdf"
    section_9 = "/Users/I764233/uni/RSA/docs/section_9_updated.pdf"
    output_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_FINAL_v5.pdf"

    reader = PdfReader(source_pdf)
    writer = PdfWriter()

    total_pages = len(reader.pages)
    print(f"Source: {total_pages} pages")

    # Pages 1-14 (indices 0-13)
    for i in range(14):
        writer.add_page(reader.pages[i])
    print("Added pages 1-14")

    # Section 6.3 (replaces pages 15-16)
    for page in PdfReader(section_6_3).pages:
        writer.add_page(page)
    print("Added section 6.3")

    # Pages 17-19 (indices 16-18)
    for i in range(16, 19):
        writer.add_page(reader.pages[i])
    print("Added pages 17-19")

    # Section 7.6 (replaces pages 20-21)
    for page in PdfReader(section_7_6).pages:
        writer.add_page(page)
    print("Added section 7.6")

    # Section 8 (replaces pages 22-24)
    for page in PdfReader(section_8).pages:
        writer.add_page(page)
    print("Added section 8")

    # Section 9 (replaces page 25)
    for page in PdfReader(section_9).pages:
        writer.add_page(page)
    print("Added section 9")

    # Page 26 - Sources (index 25)
    writer.add_page(reader.pages[25])
    print("Added page 26 (sources)")

    with open(output_pdf, "wb") as f:
        writer.write(f)

    print(f"\nOutput: {output_pdf}")
    print(f"Pages: {len(PdfReader(output_pdf).pages)}")

if __name__ == "__main__":
    main()
