#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QWidget>
#include <cstdio>

class Viewer : public QWidget {
public:
    Viewer(const QString &path, bool video, bool stretch, bool loop)
        : stretch_(stretch), video_(video)
    {
        setWindowFlags(Qt::FramelessWindowHint);
        setStyleSheet("background: black");
        if (!video) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            image_ = reader.read();
            if (image_.isNull())
                std::fprintf(stderr, "Cannot load image: %s\n", qPrintable(reader.errorString()));
            return;
        }

        const QSize size = QApplication::primaryScreen()->size();
        width_ = size.width();
        height_ = size.height();
        const QString dimensions = QString::number(width_) + ":" + QString::number(height_);
        const QString filter = stretch
            ? "fps=30,scale=" + dimensions
            : "fps=30,scale=" + dimensions + ":force_original_aspect_ratio=decrease,"
              "pad=" + dimensions + ":(ow-iw)/2:(oh-ih)/2:black";
        QStringList args = {"-hide_banner", "-loglevel", "error", "-nostdin", "-re"};
        if (loop)
            args << "-stream_loop" << "-1";
        args << "-i" << path << "-an" << "-vf" << filter
             << "-pix_fmt" << "rgb565le" << "-f" << "rawvideo" << "pipe:1";
        connect(&decoder_, &QProcess::readyReadStandardOutput, this, [this] { readFrames(); });
        connect(&decoder_, &QProcess::readyReadStandardError, this, [this] {
            std::fprintf(stderr, "%s", decoder_.readAllStandardError().constData());
        });
        decoder_.start("ffmpeg", args);
    }

    ~Viewer() override
    {
        if (video_ && decoder_.state() != QProcess::NotRunning) {
            decoder_.terminate();
            if (!decoder_.waitForFinished(1500)) {
                decoder_.kill();
                decoder_.waitForFinished();
            }
        }
    }

    bool valid() const { return video_ || !image_.isNull(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        if (image_.isNull())
            return;
        const QRect target = stretch_ ? rect() : QRect(QPoint(0, 0), image_.size().scaled(size(), Qt::KeepAspectRatio));
        const QRect centered((width() - target.width()) / 2, (height() - target.height()) / 2,
                             target.width(), target.height());
        painter.drawImage(centered, image_);
    }

private:
    void readFrames()
    {
        pending_ += decoder_.readAllStandardOutput();
        const qsizetype frameBytes = qsizetype(width_) * height_ * 2;
        while (pending_.size() >= frameBytes) {
            // Copy before removing the data backing the frame.
            image_ = QImage(reinterpret_cast<const uchar *>(pending_.constData()),
                            width_, height_, QImage::Format_RGB16).copy();
            pending_.remove(0, frameBytes);
        }
        update();
    }

    bool stretch_;
    bool video_;
    int width_ = 0;
    int height_ = 0;
    QImage image_;
    QByteArray pending_;
    QProcess decoder_;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("dbc-media-viewer");
    QCommandLineParser parser;
    parser.setApplicationDescription("Fullscreen MP4, JPEG and PNG mock-up viewer");
    parser.addHelpOption();
    QCommandLineOption stretchOption("stretch", "Stretch to fill the display (default: preserve aspect ratio).");
    QCommandLineOption onceOption("once", "Play MP4 only once (default: loop).");
    parser.addOption(stretchOption);
    parser.addOption(onceOption);
    parser.addPositionalArgument("file", "Local .mp4, .jpg, .jpeg or .png file.");
    parser.process(app);
    if (parser.positionalArguments().size() != 1)
        parser.showHelp(2);

    const QFileInfo file(parser.positionalArguments().first());
    const QString extension = file.suffix().toLower();
    const bool video = extension == "mp4";
    if (!file.isFile() || !file.isReadable() ||
        (!video && extension != "jpg" && extension != "jpeg" && extension != "png")) {
        std::fprintf(stderr, "Expected a readable MP4, JPEG or PNG file: %s\n",
                     qPrintable(file.absoluteFilePath()));
        return 2;
    }

    Viewer viewer(file.absoluteFilePath(), video, parser.isSet(stretchOption), !parser.isSet(onceOption));
    if (!viewer.valid())
        return 1;
    viewer.showFullScreen();
    return app.exec();
}
