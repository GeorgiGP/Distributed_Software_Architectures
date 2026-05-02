#!/usr/bin/env python3
"""
Replace pages in the main PDF with updated sections.
"""

from pypdf import PdfReader, PdfWriter

def main():
    # Read the source PDF
    source_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_0MI0600299_v3.pdf"
    output_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_FINAL_v2.pdf"

    # New content PDFs
    section_6_3 = "/Users/I764233/uni/RSA/docs/section_6_3_new.pdf"
    section_7_6 = "/Users/I764233/uni/RSA/docs/section_7_6_new.pdf"

    reader = PdfReader(source_pdf)
    writer = PdfWriter()

    total_pages = len(reader.pages)
    print(f"Source PDF has {total_pages} pages")

    # Copy pages 1-14 (indices 0-13)
    for i in range(14):
        writer.add_page(reader.pages[i])
        print(f"Added original page {i+1}")

    # Insert section 6.3 (replaces page 15)
    section_6_3_reader = PdfReader(section_6_3)
    for page in section_6_3_reader.pages:
        writer.add_page(page)
        print(f"Added section 6.3 page")

    # Copy pages 16-18 (indices 15-17) - section 6.4 and start of 7
    for i in range(15, 18):
        writer.add_page(reader.pages[i])
        print(f"Added original page {i+1}")

    # Copy more pages until section 7.6 (approximately page 19-20)
    for i in range(18, 20):
        writer.add_page(reader.pages[i])
        print(f"Added original page {i+1}")

    # Insert section 7.6
    section_7_6_reader = PdfReader(section_7_6)
    for page in section_7_6_reader.pages:
        writer.add_page(page)
        print(f"Added section 7.6 page")

    # Copy remaining pages (from original page 21 onwards)
    for i in range(22, total_pages):
        writer.add_page(reader.pages[i])
        print(f"Added original page {i+1}")

    # Write output
    with open(output_pdf, "wb") as f:
        writer.write(f)

    print(f"\nOutput written to: {output_pdf}")
    print(f"Total pages in output: {len(writer.pages)}")

if __name__ == "__main__":
    main()
