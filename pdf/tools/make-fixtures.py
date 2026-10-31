#!/usr/bin/env python3
"""Writes the small PDFs the tests open.

Hand-written PDF so the fixtures need nothing but Python to rebuild:
lease.pdf (three pages of text) and form.pdf (one page with a form).
Run from the repository root: python3 tools/make-fixtures.py
"""
import pathlib

OUT = pathlib.Path(__file__).resolve().parent.parent / "tests" / "fixtures"


def write_pdf(path, objects):
    """objects: list of bytes bodies, object n is objects[n-1]; 1 is the catalog."""
    out = bytearray(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for number, body in enumerate(objects, start=1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % number + body + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1)
    for offset in offsets:
        out += b"%010d 00000 n \n" % offset
    out += b"trailer\n<< /Size %d /Root 1 0 R /Info %d 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (
        len(objects) + 1, len(objects), xref)
    path.write_bytes(out)


def stream(content, extra=b""):
    return b"<< /Length %d %s>>\nstream\n" % (len(content), extra) + content + b"\nendstream"


def text_page(lines):
    # Each line moves down before it is shown, so the first lands at 720.
    ops = [b"BT /F1 12 Tf 72 736 Td 16 TL"]
    for line in lines:
        ops.append(b"(" + line.encode("latin-1") + b") '")
    ops.append(b"ET")
    return b"\n".join(ops)


def lease():
    pages = [
        ["Residential lease", "1. Parties", "This lease is between the Landlord and the Tenant."],
        ["Security deposit and rent",
         "3.1 The Tenant pays a security deposit equal to one month's rent.",
         "3.2 Rent is due on the first day of each month.",
         "The deposit is returned within 21 days of the lease ending."],
        ["Signatures", "By signing below, both parties agree to every term.",
         "No deposit is due after this page."],
    ]
    # 1 catalog, 2 pages, 3 font, then (page, content) pairs, then info.
    objects = [b"", b"", b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"]
    kids = []
    for lines in pages:
        page_no = len(objects) + 1
        kids.append(page_no)
        objects.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                       b"/Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>" % (page_no + 1))
        objects.append(stream(text_page(lines)))
    objects.append(b"<< /Title (Lease agreement) /Producer (good-input-pdf fixtures) >>")
    objects[0] = b"<< /Type /Catalog /Pages 2 0 R >>"
    objects[1] = b"<< /Type /Pages /Kids [%s] /Count %d >>" % (
        b" ".join(b"%d 0 R" % k for k in kids), len(kids))
    write_pdf(OUT / "lease.pdf", objects)


def form():
    # 1 catalog, 2 pages, 3 font, 4 page, 5 content, 6 name, 7 address,
    # 8 city, 9 permanent, 10 on-look, 11 off-look, 12 info
    content = text_page(["Change of address", "", "Full name", "", "", "New address", "", "",
                         "City", "", "", "Permanent move"])
    def text_field(name, rect, required):
        flags = 2 if required else 0
        return (b"<< /Type /Annot /Subtype /Widget /FT /Tx /T (%s) /TU (%s) /Ff %d "
                b"/Rect [%s] /P 4 0 R /F 4 /DA (/Helv 11 Tf 0 g) /V () >>" % (
                    name.encode(), name.replace("_", " ").capitalize().encode(), flags, rect))
    check_look = b"q 0 g BT /ZaDb 10 Tf 2 3 Td (4) Tj ET Q"
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [6 0 R 7 0 R 8 0 R 9 0 R] "
        b"/NeedAppearances true /DA (/Helv 11 Tf 0 g) "
        b"/DR << /Font << /Helv 3 0 R /ZaDb << /Type /Font /Subtype /Type1 /BaseFont /ZapfDingbats >> >> >> >> >>",
        b"<< /Type /Pages /Kids [4 0 R] /Count 1 >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> "
        b"/Contents 5 0 R /Annots [6 0 R 7 0 R 8 0 R 9 0 R] >>",
        stream(content),
        text_field("full_name", b"72 660 400 680", True),
        text_field("new_address", b"72 612 400 632", True),
        text_field("city", b"72 564 300 584", False),
        b"<< /Type /Annot /Subtype /Widget /FT /Btn /T (permanent) /TU (Permanent move) "
        b"/Rect [72 520 86 534] /P 4 0 R /F 4 /V /Off /AS /Off "
        b"/MK << /CA (4) >> /AP << /N << /Yes 10 0 R /Off 11 0 R >> >> >>",
        stream(check_look, b"/Type /XObject /Subtype /Form /BBox [0 0 14 14] "
                           b"/Resources << /Font << /ZaDb << /Type /Font /Subtype /Type1 /BaseFont /ZapfDingbats >> >> >> "),
        stream(b"", b"/Type /XObject /Subtype /Form /BBox [0 0 14 14] "),
        b"<< /Title (Change of address) >>",
    ]
    write_pdf(OUT / "form.pdf", objects)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    lease()
    form()
    (OUT / "not-a-pdf.pdf").write_text("This is plain text with a PDF name.\n")
