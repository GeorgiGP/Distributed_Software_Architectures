#!/usr/bin/env python3
"""
Merge PDFs: replace section 6.3 (page 15) and section 7.6 (page 20) with corrected versions.
"""

from pypdf import PdfReader, PdfWriter

def main():
    source_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_0MI0600299_v3.pdf"
    section_6_3 = "/Users/I764233/uni/RSA/docs/section_6_3_corrected.pdf"
    section_7_6 = "/Users/I764233/uni/RSA/docs/section_7_6_corrected.pdf"
    output_pdf = "/Users/I764233/uni/RSA/P2P_Dynamic_Balancing_FINAL_v2.pdf"

    reader = PdfReader(source_pdf)
    writer = PdfWriter()

    total_pages = len(reader.pages)
    print(f"Source PDF: {total_pages} pages")

    # Page structure (0-indexed):
    # 0-13: pages 1-14 (keep)
    # 14-15: pages 15-16 (section 6.3) - REPLACE with corrected
    # 16-18: pages 17-19 (keep)
    # 19-20: pages 20-21 (section 7.6) - REPLACE with corrected
    # 21+: pages 22+ (keep)

    # Copy pages 1-14 (indices 0-13)
    for i in range(14):
        writer.add_page(reader.pages[i])
    print("Added pages 1-14 from original")

    # Insert corrected section 6.3 (replaces pages 15-16)
    section_6_3_reader = PdfReader(section_6_3)
    for page in section_6_3_reader.pages:
        writer.add_page(page)
    print(f"Added {len(section_6_3_reader.pages)} page(s) for section 6.3")

    # Copy pages 17-19 (indices 16-18)
    for i in range(16, 19):
        writer.add_page(reader.pages[i])
    print("Added pages 17-19 from original")

    # Insert corrected section 7.6 (replaces pages 20-21)
    section_7_6_reader = PdfReader(section_7_6)
    for page in section_7_6_reader.pages:
        writer.add_page(page)
    print(f"Added {len(section_7_6_reader.pages)} page(s) for section 7.6")

    # Copy remaining pages (22 onwards, index 21+)
    for i in range(21, total_pages):
        writer.add_page(reader.pages[i])
    print(f"Added pages 22-{total_pages} from original")

    # Write output
    with open(output_pdf, "wb") as f:
        writer.write(f)

    final_reader = PdfReader(output_pdf)
    print(f"\nOutput: {output_pdf}")
    print(f"Total pages: {len(final_reader.pages)}")

if __name__ == "__main__":
    main()
