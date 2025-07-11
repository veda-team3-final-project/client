#ifndef LINEDRAWINGDIALOG_H
#define LINEDRAWINGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSlider>
#include <QTimer>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QList>
#include <QPair>
#include <QTextEdit>
#include <QTime>
#include <QResizeEvent>

// 비디오 오버레이 위젯 (선 그리기용)
class VideoOverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoOverlayWidget(QWidget *parent = nullptr);
    void setDrawingMode(bool enabled);
    void clearLines();
    QList<QPair<QPoint, QPoint>> getLines() const;

signals:
    void lineDrawn(const QPoint &start, const QPoint &end);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_drawingMode;
    bool m_drawing;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    QList<QPair<QPoint, QPoint>> m_lines;
};

class LineDrawingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LineDrawingDialog(const QString &rtspUrl, QWidget *parent = nullptr);
    ~LineDrawingDialog();

signals:
    void lineCoordinatesReady(int x1, int y1, int x2, int y2);

private slots:
    void onStartDrawingClicked();
    void onStopDrawingClicked();
    void onClearLinesClicked();
    void onSendCoordinatesClicked();
    void onLineDrawn(const QPoint &start, const QPoint &end);
    void onClearLogClicked();
    void onPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void updateFrameCount();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void setupMediaPlayer();
    void startVideoStream();
    void stopVideoStream();
    void addLogMessage(const QString &message, const QString &type = "INFO");
    void clearLog();
    void updateButtonStates();

    // UI 컴포넌트 (헤더 선언 순서대로 정렬)
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_buttonLayout;
    QVideoWidget *m_videoWidget;
    VideoOverlayWidget *m_overlayWidget;
    QPushButton *m_startDrawingButton;
    QPushButton *m_stopDrawingButton;
    QPushButton *m_clearLinesButton;
    QPushButton *m_sendCoordinatesButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QLabel *m_frameCountLabel;

    // 로그 관련 UI
    QTextEdit *m_logTextEdit;
    QLabel *m_logCountLabel;
    QPushButton *m_clearLogButton;

    // 미디어 관련
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;

    // 상태 관리
    QString m_rtspUrl;
    QList<QPair<QPoint, QPoint>> m_drawnLines;
    bool m_isDrawingMode;
    QTimer *m_frameTimer;
    int m_frameCount;
};

#endif // LINEDRAWINGDIALOG_H
