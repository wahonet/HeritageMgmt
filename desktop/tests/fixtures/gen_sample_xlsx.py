# 生成 tests/fixtures/sample.xlsx（最小但合法的 .xlsx，ZIP_DEFLATE 压缩）。
# 用 Python 标准库 zipfile + 手写 XML，零第三方依赖。供 excel_test 验证自写 xlsx 读取器。
import os, zipfile

ROWS = [
    ["序号", "项目名称", "财政指标文下达金额", "工程合同金额", "已支付金额", "项目建设情况"],
    ["1", "大雄宝殿修缮工程", "350.5", "300", "120", "在建"],
    ["2", "千佛崖抢险加固", "1200", "1100", "1200", "已竣工验收"],
    ["小计", "", "", "", "", ""],            # 非数字序号 → 应被 loadFinancials 过滤
    ["3", "天贶殿保护", "800", "750", "100", "在建"],
]

NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"

# shared strings
sst = []
def idx(s):
    if s not in sst:
        sst.append(s)
    return sst.index(s)

def cell(col_i, rownum, val):
    col = chr(ord("A") + col_i)
    return f'<c r="{col}{rownum}" t="s"><v>{idx(val)}</v></c>'

sheet_rows = []
for ri, row in enumerate(ROWS, start=1):
    sheet_rows.append("<row r=\"%d\">%s</row>" % (ri, "".join(cell(ci, ri, v) for ci, v in enumerate(row))))
sheet_xml = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    f'<worksheet xmlns="{NS}"><sheetData>' + "".join(sheet_rows) + "</sheetData></worksheet>"
)
sst_xml = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    f'<sst xmlns="{NS}" count="{sum(len(r) for r in ROWS)}" uniqueCount="{len(sst)}">'
    + "".join(f"<si><t>{s}</t></si>" for s in sst) + "</sst>"
)

CONTENT_TYPES = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
    '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
    '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>'
    "</Types>"
)
RELS = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
    "</Relationships>"
)
WORKBOOK = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    f'<workbook xmlns="{NS}" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
    '<sheets><sheet name="Sheet1" sheetId="1" r:id="rId1"/></sheets></workbook>'
)
WB_RELS = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>'
    '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>'
    "</Relationships>"
)

out = os.path.join(os.path.dirname(__file__), "sample.xlsx")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", CONTENT_TYPES)
    z.writestr("_rels/.rels", RELS)
    z.writestr("xl/workbook.xml", WORKBOOK)
    z.writestr("xl/_rels/workbook.xml.rels", WB_RELS)
    z.writestr("xl/worksheets/sheet1.xml", sheet_xml)
    z.writestr("xl/sharedStrings.xml", sst_xml)
print("wrote", out, os.path.getsize(out), "bytes")
