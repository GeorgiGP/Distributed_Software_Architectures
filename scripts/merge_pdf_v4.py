#!/usr/bin/env python3
from pypdf import PdfReader, PdfWriter

def main():
    source_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_0MI0600299_v3.pdf"
    section_6_3 = "/Users/I764233/uni/RSA/docs/section_6_3_wide.pdf"
    section_7_6 = "/Users/I764233/uni/RSA/docs/section_7_6_wide.pdf"
    output_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_FINAL_v4.pdf"

    reader = PdfReader(source_pdf)
    writer = PdfWriter()

    total_pages = len(reader.pages)
    print(f"Source: {total_pages} pages")

    # Pages 1-14
    for i in range(14):
        writer.add_page(reader.pages[i])

    # Section 6.3
    for page in PdfReader(section_6_3).pages:
        writer.add_page(page)

    # Pages 17-19
    for i in range(16, 19):
        writer.add_page(reader.pages[i])

    # Section 7.6
    for page in PdfReader(section_7_6).pages:
        writer.add_page(page)

    # Pages 22+
    for i in range(21, total_pages):
        writer.add_page(reader.pages[i])

    with open(output_pdf, "wb") as f:
        writer.write(f)

    print(f"Output: {output_pdf}")
    print(f"Pages: {len(PdfReader(output_pdf).pages)}")

if __name__ == "__main__":
    main()
