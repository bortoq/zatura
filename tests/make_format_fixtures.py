#!/usr/bin/env python3
"""Small self-contained documents for plugin rendering smoke checks."""
import pathlib, struct, sys, tarfile, zipfile, zlib
out=pathlib.Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
def chunk(t,d): return struct.pack('!I',len(d))+t+d+struct.pack('!I',zlib.crc32(t+d)&0xffffffff)
(out/'page.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',64,64,8,2,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+b'\xc0\x40\x20'*64)*64))+chunk(b'IEND',b''))
(out/'book.fb2').write_text('''<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>science</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>FB2 fixture</book-title><lang>en</lang></title-info></description><body><section><title><p>First chapter</p></title><p>Book fixture text.</p></section></body></FictionBook>''')
with zipfile.ZipFile(out/'book.epub','w') as z:
 z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
 z.writestr('META-INF/container.xml','''<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>''')
 z.writestr('content.opf','''<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">zatura-test</dc:identifier><dc:title>EPUB fixture</dc:title><dc:language>en</dc:language></metadata><manifest><item id="one" href="one.xhtml" media-type="application/xhtml+xml"/><item id="two" href="two.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="one"/><itemref idref="two"/></spine></package>''')
 for name in ('one','two'):
  z.writestr(name+'.xhtml',f'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{name}</title></head><body><h1>Chapter {name}</h1><p>EPUB fixture text.</p></body></html>')
with zipfile.ZipFile(out/'comic.cbz','w') as z: z.write(out/'page.png','001.png')
with tarfile.open(out/'comic.cbt','w') as z: z.add(out/'page.png','001.png')
(out/'page.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" fill="#c04020"/></svg>')
(out/'page.ps').write_text('''%!PS-Adobe-3.0
%%Pages: 1
%%BoundingBox: 0 0 300 300
%%EndComments
%%Page: 1 1
0.75 0.25 0.125 setrgbcolor
20 20 200 200 rectfill
/Courier findfont 20 scalefont setfont
20 250 moveto (PostScript test) show
showpage
%%EOF
''')
with zipfile.ZipFile(out/'page.xps','w') as z:
 z.writestr('[Content_Types].xml','''<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/><Default Extension="fdoc" ContentType="application/vnd.ms-package.xps-fixeddocument+xml"/><Default Extension="fpage" ContentType="application/vnd.ms-package.xps-fixedpage+xml"/></Types>''')
 z.writestr('_rels/.rels','''<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="r1" Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" Target="/FixedDocumentSequence.fdseq"/></Relationships>''')
 z.writestr('FixedDocumentSequence.fdseq','''<FixedDocumentSequence xmlns="http://schemas.microsoft.com/xps/2005/06"><DocumentReference Source="/Documents/1.fdoc"/></FixedDocumentSequence>''')
 z.writestr('Documents/1.fdoc','''<FixedDocument xmlns="http://schemas.microsoft.com/xps/2005/06"><PageContent Source="/Pages/1.fpage" Width="64" Height="64"/></FixedDocument>''')
 z.writestr('Pages/1.fpage','''<FixedPage xmlns="http://schemas.microsoft.com/xps/2005/06" Width="64" Height="64" xml:lang="en-US"><Path Fill="#FFC04020" Data="M 0,0 L 64,0 64,64 0,64 Z"/></FixedPage>''')
(out/'page.ppm').write_bytes(b'P6\n64 64\n255\n'+b'\xc0\x40\x20'*(64*64))

for name in ('book.fb2.zip', 'collection.zip', 'BOOK.FB2Z'):
 with zipfile.ZipFile(out/name, 'w', compression=zipfile.ZIP_DEFLATED) as z:
  z.writestr('readme.txt', 'FB2 ZIP fixture')
  z.write(out/'page.png', 'cover.png')
  z.write(out/'book.fb2', 'nested/Book-α.FB2')
  z.writestr('second.fb2', '<invalid>Must not be selected</invalid>')

with zipfile.ZipFile(out/'multiple.zip', 'w', compression=zipfile.ZIP_DEFLATED) as z:
 z.write(out/'book.fb2', 'zzz-first.fb2')
 z.writestr('aaa-second.fb2', 'Invalid second book')
