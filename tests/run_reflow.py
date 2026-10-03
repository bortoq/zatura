#!/usr/bin/env python3
import pathlib, subprocess, sys, tempfile, zipfile
with tempfile.TemporaryDirectory(prefix='zatura-reflow-') as work:
    folder = pathlib.Path(work)
    subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('make_format_fixtures.py')), work], check=True)
    paragraphs = ''.join(f'<p>MARKER{i:04d} Reflow test: this paragraph contains enough words to span several lines and track the reading position across pagination changes.</p>' for i in range(600))
    fb2 = folder / 'book.fb2'
    fb2.write_text(fb2.read_text().replace('<p>Book fixture text.</p>', paragraphs))
    with zipfile.ZipFile(folder / 'book.fb2.zip', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(fb2, 'folder/book.fb2')
    epub = folder / 'book.epub'
    with zipfile.ZipFile(epub) as original:
        files = [(item, original.read(item.filename)) for item in original.infolist()]
    with zipfile.ZipFile(epub, 'w') as archive:
        for item, data in files:
            if item.filename.endswith('.xhtml'):
                data = data.decode().replace('<p>EPUB fixture text.</p>', paragraphs).encode()
            archive.writestr(item, data)
    result = subprocess.run(sys.argv[1:] + [str(fb2), str(epub), str(folder / 'book.fb2.zip')])
    sys.exit(result.returncode)
