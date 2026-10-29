#include "ClipDialogs.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dialogs {

namespace {

QDoubleSpinBox* spin(double lo, double hi, double v, int dec = 1, const char* suffix = "", double step = 1) {
    auto* s = new QDoubleSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setSuffix(suffix);
    s->setValue(v);
    return s;
}

void setSwatch(QPushButton* b, std::uint32_t rgb) {
    b->setStyleSheet(QString("background:#%1; border:1px solid #888; min-width:60px; min-height:20px;")
                         .arg(rgb, 6, 16, QChar('0')));
}

struct EffectRow {
    nexo::EffectKind kind;
    const char* label;
    QCheckBox* on = nullptr;
    QDoubleSpinBox* amount = nullptr;
};

}  // namespace

bool editProps(QWidget* parent, const nexo::Clip& clip, nexo::TrackKind kind, nexo::FrameRate fps, nexo::ClipProps& out) {
    const nexo::ClipProps& p = clip.props;
    const bool isText = clip.kind == nexo::ClipKind::Text;
    const bool video = kind == nexo::TrackKind::Video;
    const double rate = fps.den > 0 ? double(fps.num) / double(fps.den) : 30.0;

    QDialog dlg(parent);
    dlg.setWindowTitle("Propiedades del clip");
    dlg.resize(560, 520);
    auto* root = new QVBoxLayout(&dlg);
    auto* tabs = new QTabWidget;
    root->addWidget(tabs);

    // ---- Tiempo ----
    auto* speed = spin(0.1, 16, p.speed, 2, " ×", 0.25);
    auto* reverse = new QCheckBox("Reproducir hacia atrás");
    reverse->setChecked(p.reverse);
    auto* freeze = new QCheckBox("Congelar el primer fotograma (sin audio)");
    freeze->setChecked(p.freeze);
    if (!isText) {
        auto* w = new QWidget; auto* f = new QFormLayout(w);
        f->addRow("Velocidad", speed);
        f->addRow(reverse);
        f->addRow(freeze);
        f->addRow(new QLabel("La duración del clip se ajusta para conservar el mismo tramo del medio."));
        tabs->addTab(w, "Tiempo");
    }

    // ---- Transformación ----
    auto* posX = spin(-200, 200, p.posX, 1, " %"), *posY = spin(-200, 200, p.posY, 1, " %");
    auto* scale = spin(1, 1000, p.scale, 1, " %");
    auto* rot = spin(-360, 360, p.rotation, 1, " °");
    auto* opac = spin(0, 100, p.opacity * 100, 0, " %");
    auto* cl = spin(0, 99, p.cropLeft, 1, " %"), *cr = spin(0, 99, p.cropRight, 1, " %");
    auto* ct = spin(0, 99, p.cropTop, 1, " %"), *cb = spin(0, 99, p.cropBottom, 1, " %");
    auto* flipH = new QCheckBox("Voltear horizontal"), *flipV = new QCheckBox("Voltear vertical");
    flipH->setChecked(p.flipH); flipV->setChecked(p.flipV);
    auto* table = new QTableWidget(int(p.keyframes.size()), 6);
    if (video) {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w); auto* f = new QFormLayout;
        f->addRow("Posición X (desde el centro)", posX);
        f->addRow("Posición Y (desde el centro)", posY);
        f->addRow("Escala", scale);
        f->addRow("Rotación", rot);
        f->addRow("Opacidad", opac);
        f->addRow("Recorte izquierdo", cl); f->addRow("Recorte derecho", cr);
        f->addRow("Recorte superior", ct); f->addRow("Recorte inferior", cb);
        f->addRow(flipH); f->addRow(flipV);
        v->addLayout(f);
        tabs->addTab(w, "Transformar");

        // ---- Fotogramas clave ----
        auto* k = new QWidget; auto* kv = new QVBoxLayout(k);
        kv->addWidget(new QLabel("Si hay fotogramas clave, mandan sobre los valores fijos. Tiempo en segundos desde el inicio del clip;\nentre claves se interpola linealmente (movimiento, zoom, giro y opacidad animados)."));
        table->setHorizontalHeaderLabels({"Tiempo (s)", "X %", "Y %", "Escala %", "Giro °", "Opacidad %"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        auto setRow = [&](int r, double sec, double x, double y, double sc, double ro, double op) {
            const double vals[6] = {sec, x, y, sc, ro, op};
            for (int c = 0; c < 6; ++c) table->setItem(r, c, new QTableWidgetItem(QString::number(vals[c], 'f', c == 0 ? 2 : 1)));
        };
        for (int i = 0; i < int(p.keyframes.size()); ++i) {
            const nexo::Keyframe& kf = p.keyframes[std::size_t(i)];
            setRow(i, double(kf.frame) / rate, kf.posX, kf.posY, kf.scale, kf.rotation, kf.opacity * 100);
        }
        kv->addWidget(table);
        auto* bar = new QHBoxLayout;
        auto* add = new QPushButton("Añadir clave"), *del = new QPushButton("Quitar clave seleccionada");
        bar->addWidget(add); bar->addWidget(del); bar->addStretch();
        kv->addLayout(bar);
        QObject::connect(add, &QPushButton::clicked, &dlg, [&, setRow] {
            const int r = table->rowCount();
            table->insertRow(r);
            const double last = r > 0 ? table->item(r - 1, 0)->text().toDouble() : -1.0;
            setRow(r, std::min(double(clip.duration - 1) / rate, std::max(0.0, last + 0.5)), p.posX, p.posY, p.scale, p.rotation, p.opacity * 100);
        });
        QObject::connect(del, &QPushButton::clicked, &dlg, [&] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
        tabs->addTab(k, "Fotogramas clave");
    }

    // ---- Color ----
    auto* bright = spin(-1, 1, p.brightness, 2, "", 0.05), *contrast = spin(0, 3, p.contrast, 2, "", 0.05);
    auto* sat = spin(0, 3, p.saturation, 2, "", 0.05), *temp = spin(2000, 12000, p.temperature, 0, " K", 250);
    if (video) {
        auto* w = new QWidget; auto* f = new QFormLayout(w);
        f->addRow("Brillo (−1 a 1)", bright);
        f->addRow("Contraste (1 = normal)", contrast);
        f->addRow("Saturación (1 = normal, 0 = grises)", sat);
        f->addRow("Temperatura (6500 K = neutra)", temp);
        tabs->addTab(w, "Color");
    }

    // ---- Efectos, croma, estabilizar ----
    std::vector<EffectRow> effects{
        {nexo::EffectKind::Blur, "Desenfoque"}, {nexo::EffectKind::Sharpen, "Nitidez"}, {nexo::EffectKind::Vignette, "Viñeta"},
        {nexo::EffectKind::Mosaic, "Mosaico"}, {nexo::EffectKind::Grayscale, "Blanco y negro"}, {nexo::EffectKind::Sepia, "Sepia"},
        {nexo::EffectKind::Invert, "Invertir colores"}, {nexo::EffectKind::Mirror, "Espejo"}};
    auto* chromaOn = new QCheckBox("Clave de croma (pantalla verde)");
    chromaOn->setChecked(p.chroma.enabled);
    std::uint32_t chromaRgb = p.chroma.rgb;
    auto* chromaBtn = new QPushButton;
    setSwatch(chromaBtn, chromaRgb);
    QObject::connect(chromaBtn, &QPushButton::clicked, &dlg, [&] {
        const QColor c = QColorDialog::getColor(QColor((chromaRgb >> 16) & 255, (chromaRgb >> 8) & 255, chromaRgb & 255), &dlg, "Color a quitar");
        if (c.isValid()) { chromaRgb = (c.red() << 16) | (c.green() << 8) | c.blue(); setSwatch(chromaBtn, chromaRgb); }
    });
    auto* chromaSim = spin(0, 1, p.chroma.similarity, 2, "", 0.05), *chromaBlend = spin(0, 1, p.chroma.blend, 2, "", 0.05);
    auto* stab = new QCheckBox("Estabilizar imagen");
    stab->setChecked(p.stabilize);
    if (video) {
        auto* w = new QWidget; auto* f = new QFormLayout(w);
        for (EffectRow& e : effects) {
            e.on = new QCheckBox(e.label);
            e.amount = spin(0, 1, 0.5, 2, "", 0.05);
            for (const nexo::Effect& have : p.effects) if (have.kind == e.kind) { e.on->setChecked(true); e.amount->setValue(have.amount); }
            auto* row = new QHBoxLayout; row->addWidget(e.on); row->addWidget(new QLabel("intensidad")); row->addWidget(e.amount);
            f->addRow(row);
        }
        f->addRow(stab);
        f->addRow(chromaOn);
        f->addRow("Color a quitar", chromaBtn);
        f->addRow("Similitud", chromaSim);
        f->addRow("Suavizado", chromaBlend);
        tabs->addTab(w, "Efectos y croma");
    }

    // ---- Audio ----
    auto* muted = new QCheckBox("Silenciar el audio de este clip"), *denoise = new QCheckBox("Reducir ruido de fondo"),
        *normalize = new QCheckBox("Normalizar volumen");
    muted->setChecked(p.muted); denoise->setChecked(p.denoise); normalize->setChecked(p.normalize);
    auto* pitch = spin(-12, 12, p.pitch, 1, " semitonos", 1), *bass = spin(-20, 20, p.bass, 1, " dB"), *treble = spin(-20, 20, p.treble, 1, " dB");
    if (!isText) {
        auto* w = new QWidget; auto* f = new QFormLayout(w);
        f->addRow(muted); f->addRow(denoise); f->addRow(normalize);
        f->addRow("Tono (solo a velocidad normal)", pitch);
        f->addRow("Graves", bass); f->addRow("Agudos", treble);
        tabs->addTab(w, "Audio");
    }

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    nexo::ClipProps res = p;
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, [&] {
        res = p;
        if (!isText) { res.speed = speed->value(); res.reverse = reverse->isChecked(); res.freeze = freeze->isChecked(); }
        if (video) {
            res.posX = posX->value(); res.posY = posY->value(); res.scale = scale->value(); res.rotation = rot->value();
            res.opacity = opac->value() / 100.0;
            res.cropLeft = cl->value(); res.cropRight = cr->value(); res.cropTop = ct->value(); res.cropBottom = cb->value();
            res.flipH = flipH->isChecked(); res.flipV = flipV->isChecked();
            res.brightness = bright->value(); res.contrast = contrast->value(); res.saturation = sat->value(); res.temperature = temp->value();
            res.effects.clear();
            for (const EffectRow& e : effects) if (e.on->isChecked()) res.effects.push_back({e.kind, e.amount->value()});
            res.chroma.enabled = chromaOn->isChecked(); res.chroma.rgb = chromaRgb;
            res.chroma.similarity = chromaSim->value(); res.chroma.blend = chromaBlend->value();
            res.stabilize = stab->isChecked();
            res.keyframes.clear();
            for (int r = 0; r < table->rowCount(); ++r) {
                nexo::Keyframe k;
                auto cell = [&](int c) { return table->item(r, c) ? table->item(r, c)->text().toDouble() : 0.0; };
                k.frame = nexo::Frame(std::llround(cell(0) * rate));
                k.posX = cell(1); k.posY = cell(2); k.scale = cell(3); k.rotation = cell(4); k.opacity = cell(5) / 100.0;
                res.keyframes.push_back(k);
            }
            std::sort(res.keyframes.begin(), res.keyframes.end(), [](const nexo::Keyframe& a, const nexo::Keyframe& b) { return a.frame < b.frame; });
        }
        if (!isText) {
            res.muted = muted->isChecked(); res.denoise = denoise->isChecked(); res.normalize = normalize->isChecked();
            res.pitch = pitch->value(); res.bass = bass->value(); res.treble = treble->value();
        }
        // La duración nueva (por la velocidad) la calcula el modelo; aquí se valida con la actual.
        const std::string err = nexo::validateProps(res, clip.duration);
        if (!err.empty()) { QMessageBox::warning(&dlg, "Propiedades del clip", QString::fromStdString(err)); return; }
        dlg.accept();
    });
    if (dlg.exec() != QDialog::Accepted) return false;
    out = res;
    return true;
}

bool editText(QWidget* parent, const QString& title, nexo::TextProps& t) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto* f = new QFormLayout(&dlg);
    auto* text = new QPlainTextEdit(QString::fromStdString(t.text));
    text->setMinimumHeight(70);
    auto* font = new QFontComboBox; font->setCurrentFont(QFont(QString::fromStdString(t.font)));
    auto* size = spin(1, 50, t.size, 1, " % del alto");
    auto* bold = new QCheckBox("Negrita"); bold->setChecked(t.bold);
    std::uint32_t rgb = t.color;
    auto* colorBtn = new QPushButton; setSwatch(colorBtn, rgb);
    QObject::connect(colorBtn, &QPushButton::clicked, &dlg, [&] {
        const QColor c = QColorDialog::getColor(QColor((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255), &dlg, "Color del texto");
        if (c.isValid()) { rgb = (c.red() << 16) | (c.green() << 8) | c.blue(); setSwatch(colorBtn, rgb); }
    });
    auto* px = spin(0, 100, t.posX, 1, " %"), *py = spin(0, 100, t.posY, 1, " %");
    f->addRow("Texto", text); f->addRow("Fuente", font); f->addRow("Tamaño", size); f->addRow(bold);
    f->addRow("Color", colorBtn); f->addRow("Posición horizontal (centro)", px); f->addRow("Posición vertical (centro)", py);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    f->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, [&] {
        if (text->toPlainText().trimmed().isEmpty()) { QMessageBox::warning(&dlg, title, "Escribe algún texto."); return; }
        dlg.accept();
    });
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return false;
    t.text = text->toPlainText().toStdString();
    t.font = font->currentFont().family().toStdString();
    t.size = size->value(); t.bold = bold->isChecked(); t.color = rgb; t.posX = px->value(); t.posY = py->value();
    return true;
}

bool chooseExport(QWidget* parent, nexo::ExportSettings& st, QString& outPath, const QString& defaultDir) {
    QDialog dlg(parent);
    dlg.setWindowTitle("Exportar");
    auto* f = new QFormLayout(&dlg);
    auto* fmt = new QComboBox;
    const nexo::ExportFormat fmts[] = {nexo::ExportFormat::Mp4, nexo::ExportFormat::Mov, nexo::ExportFormat::WebM, nexo::ExportFormat::Gif,
                                       nexo::ExportFormat::Mp3, nexo::ExportFormat::Wav};
    for (auto x : fmts) fmt->addItem(nexo::formatLabel(x));
    auto* res = new QComboBox;
    res->addItems({"720p (1280×720)", "1080p (1920×1080)", "480p (854×480)", "360p (640×360)"});
    auto* q = new QComboBox; q->addItems({"Alta", "Media", "Baja (archivo pequeño)"}); q->setCurrentIndex(1);
    f->addRow("Formato", fmt); f->addRow("Resolución", res); f->addRow("Calidad", q);
    auto* hint = new QLabel;
    f->addRow(hint);
    auto upd = [&] {
        const bool audio = nexo::formatIsAudioOnly(fmts[fmt->currentIndex()]);
        res->setEnabled(!audio);
        q->setEnabled(!audio && fmts[fmt->currentIndex()] != nexo::ExportFormat::Gif);
        hint->setText(fmts[fmt->currentIndex()] == nexo::ExportFormat::Gif ? "GIF: usa resolución baja (360p) o el archivo será enorme." : "");
    };
    QObject::connect(fmt, &QComboBox::currentIndexChanged, &dlg, upd);
    upd();
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    f->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return false;
    st.format = fmts[fmt->currentIndex()];
    st.quality = q->currentIndex();
    switch (res->currentIndex()) {
        case 1: st.width = 1920; st.height = 1080; break;
        case 2: st.width = 854; st.height = 480; break;
        case 3: st.width = 640; st.height = 360; break;
        default: st.width = 1280; st.height = 720;
    }
    const QString ext = nexo::formatExtension(st.format);
    outPath = QFileDialog::getSaveFileName(parent, "Exportar", defaultDir + "/nexo." + ext, QString("%1 (*.%2)").arg(nexo::formatLabel(st.format), ext));
    if (outPath.isEmpty()) return false;
    if (!outPath.endsWith("." + ext, Qt::CaseInsensitive)) outPath += "." + ext;
    return true;
}

}  // namespace dialogs
