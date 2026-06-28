#include "Report.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPdfWriter>
#include <QTextDocument>
#include <QTextStream>

namespace heritage::report {

static const QHash<QString, QString>& levelNames() {
    static const QHash<QString, QString> m = {
        {QStringLiteral("国保"), QStringLiteral("全国重点文物保护单位")},
        {QStringLiteral("省保"), QStringLiteral("省级文物保护单位")},
        {QStringLiteral("市保"), QStringLiteral("市级文物保护单位")},
        {QStringLiteral("县保"), QStringLiteral("县级文物保护单位")},
        {QStringLiteral("未定级"), QStringLiteral("未定级文物点")},
    };
    return m;
}

QString levelName(const QString& level) {
    if (level.isEmpty())
        return QStringLiteral("未定级文物点");
    return levelNames().value(level, level);
}

QString findChineseFont(const QString& appBase) {
    // 应用自带
    const QString appFont = appBase + QStringLiteral("/fonts/msyh.ttf");
    if (QFileInfo::exists(appFont))
        return appFont;
    // 系统/常见路径
    static const QStringList candidates = {
        QStringLiteral("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"),
        QStringLiteral("/usr/share/fonts/wqy-microhei/wqy-microhei.ttc"),
        QStringLiteral("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
        QStringLiteral("/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"),
        QStringLiteral("C:/Windows/Fonts/simhei.ttf"),
        QStringLiteral("C:/Windows/Fonts/msyh.ttf"),
        QStringLiteral("C:/Windows/Fonts/msyh.ttc"),
        QStringLiteral("C:/Windows/Fonts/simsun.ttc"),
    };
    for (const QString& p : candidates)
        if (QFileInfo::exists(p))
            return p;
    return {};
}

namespace {
QString dash(const QString& s) { return s.isEmpty() ? QStringLiteral("—") : s; }
QString money(const std::optional<double>& v) {
    return (v.has_value() && *v != 0.0) ? QString::number(*v, 'f', 2) : QStringLiteral("—");
}
QString payRate(const std::optional<double>& cf, const std::optional<double>& tp) {
    if (cf.has_value() && tp.has_value() && *cf > 0)
        return QString::number(*tp / *cf * 100.0, 'f', 1) + QStringLiteral("%");
    return QStringLiteral("—");
}
QString esc(const QString& s) { return s.toHtmlEscaped(); }

// 一行 KV（HTML 表格行）
QString kv(const QString& k, const QString& v) {
    return QStringLiteral("<tr><td style='color:#787878;width:130px;'>") + k +
           QStringLiteral("：</td><td>") + esc(v) + QStringLiteral("</td></tr>");
}
} // namespace

bool generateReport(const ReportData& rd, const QString& outPath, QString* err) {
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("无法写入文件：") + outPath;
        return false;
    }

    const Project& p = rd.project;
    QString html = QStringLiteral(
        "<html><head><meta charset='utf-8'><style>"
        "body{font-family:'Microsoft YaHei','SimSun','WenQuanYi Micro Hei','Noto Sans CJK SC',sans-serif;}"
        "h1{color:#6b4423;} h2{color:#6b4423;border-bottom:1px solid #ddd;padding-bottom:4px;}"
        "table{border-collapse:collapse;} td{padding:2px 6px;font-size:11pt;}"
        ".muted{color:#969696;font-size:10pt;} .cover{margin-top:40px;}"
        "</style></head><body>");

    // 封面
    html += QStringLiteral("<div class='cover'><h1 style='font-size:22pt;'>文物保护工程管理报告</h1>");
    html += QStringLiteral("<p class='muted'>生成日期：") +
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy年MM月dd日")) +
            QStringLiteral("</p></div>");

    // 一、基本信息
    html += QStringLiteral("<h2>一、工程基本信息</h2><table>");
    html += kv(QStringLiteral("工程名称"), dash(p.name));
    html += kv(QStringLiteral("文物单位"), dash(p.unitName));
    html += kv(QStringLiteral("保护级别"), levelName(rd.unitLevel));
    html += kv(QStringLiteral("工程类型"), dash(p.ptype));
    html += kv(QStringLiteral("当前状态"), dash(p.status));
    html += kv(QStringLiteral("批复文号"), dash(p.approvalNo));
    html += kv(QStringLiteral("合同编号"), dash(p.contractNo));
    html += kv(QStringLiteral("建设单位"), dash(p.ownerUnit));
    html += kv(QStringLiteral("施工单位"), dash(p.constructionUnit) + QStringLiteral("（资质：") + dash(p.constructionQual) + QStringLiteral("）"));
    html += kv(QStringLiteral("设计单位"), dash(p.designUnit) + QStringLiteral("（资质：") + dash(p.designQual) + QStringLiteral("）"));
    html += kv(QStringLiteral("监理单位"), dash(p.supervisionUnit) + QStringLiteral("（资质：") + dash(p.supervisionQual) + QStringLiteral("）"));
    html += QStringLiteral("</table>");

    // 二、资金
    html += QStringLiteral("<h2>二、资金执行情况（万元）</h2><table>");
    html += kv(QStringLiteral("中央指标"), money(p.centralFunding));
    html += kv(QStringLiteral("工程合同价"), money(p.engContract));
    html += kv(QStringLiteral("监理合同价"), money(p.supContract));
    html += kv(QStringLiteral("设计合同价"), money(p.desContract));
    html += kv(QStringLiteral("已支付合计"), money(p.totalPaid));
    html += kv(QStringLiteral("整体支付率"), payRate(p.centralFunding, p.totalPaid));
    html += kv(QStringLiteral("合同工期"), dash(p.signDate) + QStringLiteral(" 至 ") + dash(p.contractEnd));
    html += kv(QStringLiteral("实际完工"), dash(p.completeDate));
    html += QStringLiteral("</table>");

    // 三、档案完整度
    html += QStringLiteral("<h2>三、档案完整度</h2><table>");
    html += kv(QStringLiteral("必备材料完整度"), QString::number(rd.completeness) + QStringLiteral("%"));
    html += kv(QStringLiteral("缺项材料"),
               rd.missingRequired.isEmpty() ? QStringLiteral("无（全部齐全）")
                                            : rd.missingRequired.join(QStringLiteral("、")));
    html += QStringLiteral("</table>");

    // 四、智能分析
    html += QStringLiteral("<h2>四、智能分析报告</h2>");
    html += QStringLiteral("<p class='muted'>（以下分析由人工智能基于工程数据生成，供参考）</p>");
    html += QStringLiteral("<p style='font-size:11pt;'>") +
            esc(rd.analysis.isEmpty() ? QStringLiteral("（未配置大模型，分析报告功能不可用。）") : rd.analysis) +
            QStringLiteral("</p>");

    html += QStringLiteral("</body></html>");

    // 字体：随程序/系统 CJK 字体优先
    QFont font;
    const QString fontPath = findChineseFont(QString()); // appBase 暂用系统路径
    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        if (id >= 0) {
            const QStringList fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty())
                font.setFamily(fams.first());
        }
    }

    QPdfWriter pdf(&file);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(96);
    QTextDocument doc;
    if (!font.family().isEmpty())
        doc.setDefaultFont(font);
    doc.setHtml(html);
    doc.print(&pdf);
    file.close();
    return true;
}

} // namespace heritage::report
