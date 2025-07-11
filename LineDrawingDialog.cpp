#include "LineDrawingDialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QUrl>
#include <QTextCursor>
#include <QTextDocument>

// VideoOverlayWidget 구현
VideoOverlayWidget::VideoOverlayWidget(QWidget *parent)
    : QWidget(parent)
    , m_drawingMode(false)
    , m_drawing(false)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("background: transparent;");
}

void VideoOverlayWidget::setDrawingMode(bool enabled)
{
    m_drawingMode = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void VideoOverlayWidget::clearLines()
{
    m_lines.clear();
    update();
}

QList<QPair<QPoint, QPoint>> VideoOverlayWidget::getLines() const
{
    return m_lines;
}

void VideoOverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_drawingMode || event->button() != Qt::LeftButton) {
        return;
    }

    m_drawing = true;
    m_startPoint = event->pos();
    m_currentPoint = event->pos();
    update();
}

void VideoOverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawingMode || !m_drawing) {
        return;
    }

    m_currentPoint = event->pos();
    update();
}

void VideoOverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_drawingMode || !m_drawing || event->button() != Qt::LeftButton) {
        return;
    }

    m_drawing = false;
    QPoint endPoint = event->pos();

    // 최소 거리 체크 (너무 짧은 선은 무시)
    if ((endPoint - m_startPoint).manhattanLength() > 10) {
        m_lines.append(qMakePair(m_startPoint, endPoint));
        emit lineDrawn(m_startPoint, endPoint);
        update();
    }
}

void VideoOverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 저장된 선들 그리기
    QPen linePen(Qt::red, 3, Qt::SolidLine);
    painter.setPen(linePen);

    for (const auto &line : m_lines) {
        painter.drawLine(line.first, line.second);

        // 시작점과 끝점에 작은 원 그리기
        painter.setBrush(Qt::red);
        painter.drawEllipse(line.first, 6, 6);
        painter.drawEllipse(line.second, 6, 6);
        painter.setBrush(Qt::NoBrush);
    }

    // 현재 그리고 있는 선 그리기
    if (m_drawing && m_drawingMode) {
        QPen currentPen(Qt::yellow, 2, Qt::DashLine);
        painter.setPen(currentPen);
        painter.drawLine(m_startPoint, m_currentPoint);

        painter.setBrush(Qt::yellow);
        painter.drawEllipse(m_startPoint, 4, 4);
        painter.setBrush(Qt::NoBrush);
    }
}

// LineDrawingDialog 구현
LineDrawingDialog::LineDrawingDialog(const QString &rtspUrl, QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_videoWidget(nullptr)
    , m_overlayWidget(nullptr)
    , m_startDrawingButton(nullptr)
    , m_stopDrawingButton(nullptr)
    , m_clearLinesButton(nullptr)
    , m_sendCoordinatesButton(nullptr)
    , m_closeButton(nullptr)
    , m_statusLabel(nullptr)
    , m_frameCountLabel(nullptr)
    , m_logTextEdit(nullptr)
    , m_logCountLabel(nullptr)
    , m_clearLogButton(nullptr)
    , m_mediaPlayer(nullptr)
    , m_audioOutput(nullptr)
    , m_rtspUrl(rtspUrl)
    , m_drawnLines()
    , m_isDrawingMode(false)
    , m_frameTimer(nullptr)
    , m_frameCount(0)
{
    setWindowTitle("기준선 그리기");
    setModal(true);
    resize(1200, 700);

    setupUI();
    setupMediaPlayer();
    startVideoStream();
}

LineDrawingDialog::~LineDrawingDialog()
{
    stopVideoStream();
    if (m_mediaPlayer) {
        delete m_mediaPlayer;
    }
    if (m_audioOutput) {
        delete m_audioOutput;
    }
}

void LineDrawingDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);

    // 메인 콘텐츠 영역 (비디오 + 로그)
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // 왼쪽: 비디오 영역
    QWidget *videoContainer = new QWidget();
    videoContainer->setMinimumSize(640, 480);
    videoContainer->setStyleSheet("background-color: black; border: 2px solid #ddd; border-radius: 8px;");

    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(5, 5, 5, 5);
    videoLayout->setSpacing(5);

    // 비디오 위젯 생성
    m_videoWidget = new QVideoWidget(videoContainer);
    m_videoWidget->setMinimumSize(630, 470);
    m_videoWidget->setStyleSheet("background-color: black; border-radius: 5px;");
    videoLayout->addWidget(m_videoWidget);

    // 오버레이 위젯 설정 - 비디오 위젯과 정확히 같은 크기와 위치로 설정
    m_overlayWidget = new VideoOverlayWidget(videoContainer);

    // 초기 위치 설정 - 비디오 위젯과 같은 위치에 배치
    m_overlayWidget->setGeometry(m_videoWidget->geometry());
    m_overlayWidget->raise();

    connect(m_overlayWidget, &VideoOverlayWidget::lineDrawn, this, &LineDrawingDialog::onLineDrawn);

    contentLayout->addWidget(videoContainer, 2);

    // 오른쪽: 로그 영역
    QWidget *logContainer = new QWidget();
    logContainer->setMinimumWidth(350);
    logContainer->setMaximumWidth(400);
    logContainer->setStyleSheet("background-color: #f8f9fa; border: 2px solid #ddd; border-radius: 8px;");

    QVBoxLayout *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(10, 10, 10, 10);
    logLayout->setSpacing(8);

    // 로그 헤더
    QLabel *logHeaderLabel = new QLabel("📋 작업 로그");
    logHeaderLabel->setStyleSheet("color: #333; font-size: 16px; font-weight: bold; padding: 5px;");
    logLayout->addWidget(logHeaderLabel);

    // 로그 카운트 라벨
    m_logCountLabel = new QLabel("로그: 0개");
    m_logCountLabel->setStyleSheet("color: #666; font-size: 12px; padding: 2px;");
    logLayout->addWidget(m_logCountLabel);

    // 로그 텍스트 영역
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setStyleSheet(
        "QTextEdit { "
        "background-color: white; "
        "border: 1px solid #ccc; "
        "border-radius: 5px; "
        "padding: 8px; "
        "font-family: 'Consolas', 'Monaco', monospace; "
        "font-size: 11px; "
        "}"
        );
    logLayout->addWidget(m_logTextEdit);

    // 로그 지우기 버튼
    m_clearLogButton = new QPushButton("🗑️ 로그 지우기");
    m_clearLogButton->setStyleSheet(
        "QPushButton { "
        "background-color: #6c757d; "
        "color: white; "
        "padding: 8px 15px; "
        "border: none; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "background-color: #5a6268; "
        "}"
        );
    connect(m_clearLogButton, &QPushButton::clicked, this, &LineDrawingDialog::onClearLogClicked);
    logLayout->addWidget(m_clearLogButton);

    contentLayout->addWidget(logContainer, 1);

    m_mainLayout->addLayout(contentLayout);

    // 상태 정보
    m_statusLabel = new QLabel("비디오 스트림 연결 중...");
    m_statusLabel->setStyleSheet("color: blue; font-weight: bold; padding: 5px;");
    m_mainLayout->addWidget(m_statusLabel);

    m_frameCountLabel = new QLabel("프레임: 0");
    m_frameCountLabel->setStyleSheet("color: gray; padding: 2px;");
    m_mainLayout->addWidget(m_frameCountLabel);

    // 버튼 영역
    m_buttonLayout = new QHBoxLayout();

    m_startDrawingButton = new QPushButton("🖊️ 그리기 시작");
    m_startDrawingButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover { background-color: #45a049; }");
    connect(m_startDrawingButton, &QPushButton::clicked, this, &LineDrawingDialog::onStartDrawingClicked);
    m_buttonLayout->addWidget(m_startDrawingButton);

    m_stopDrawingButton = new QPushButton("⏹️ 그리기 중지");
    m_stopDrawingButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover { background-color: #da190b; }");
    m_stopDrawingButton->setEnabled(false);
    connect(m_stopDrawingButton, &QPushButton::clicked, this, &LineDrawingDialog::onStopDrawingClicked);
    m_buttonLayout->addWidget(m_stopDrawingButton);

    m_clearLinesButton = new QPushButton("🗑️ 선 지우기");
    m_clearLinesButton->setStyleSheet("QPushButton { background-color: #ff9800; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover { background-color: #e68900; }");
    connect(m_clearLinesButton, &QPushButton::clicked, this, &LineDrawingDialog::onClearLinesClicked);
    m_buttonLayout->addWidget(m_clearLinesButton);

    m_sendCoordinatesButton = new QPushButton("📤 좌표 전송");
    m_sendCoordinatesButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover { background-color: #1976D2; }");
    connect(m_sendCoordinatesButton, &QPushButton::clicked, this, &LineDrawingDialog::onSendCoordinatesClicked);
    m_buttonLayout->addWidget(m_sendCoordinatesButton);

    m_buttonLayout->addStretch();

    m_closeButton = new QPushButton("❌ 닫기");
    m_closeButton->setStyleSheet("QPushButton { background-color: #9E9E9E; color: white; padding: 10px 20px; border: none; border-radius: 5px; font-weight: bold; } QPushButton:hover { background-color: #757575; }");
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    m_buttonLayout->addWidget(m_closeButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // 프레임 카운터 타이머
    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, &LineDrawingDialog::updateFrameCount);
    m_frameTimer->start(1000);

    // 초기 로그 메시지
    addLogMessage("기준선 그리기 다이얼로그가 시작되었습니다.", "SYSTEM");
}

void LineDrawingDialog::setupMediaPlayer()
{
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_mediaPlayer->setVideoOutput(m_videoWidget);

    // 볼륨 설정 (0으로 설정하여 소리 끄기)
    m_audioOutput->setVolume(0.0);

    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &LineDrawingDialog::onPlayerStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this, &LineDrawingDialog::onPlayerError);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &LineDrawingDialog::onMediaStatusChanged);

    qDebug() << "미디어 플레이어 설정 완료";
}

void LineDrawingDialog::startVideoStream()
{
    if (!m_rtspUrl.isEmpty()) {
        qDebug() << "RTSP 스트림 시작:" << m_rtspUrl;
        m_mediaPlayer->setSource(QUrl(m_rtspUrl));
        m_mediaPlayer->play();
        m_statusLabel->setText("비디오 스트림 연결 중...");
    } else {
        m_statusLabel->setText("RTSP URL이 설정되지 않았습니다.");
        qDebug() << "RTSP URL이 비어있습니다.";
    }
}

void LineDrawingDialog::stopVideoStream()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
    if (m_frameTimer) {
        m_frameTimer->stop();
    }
}

void LineDrawingDialog::onStartDrawingClicked()
{
    m_isDrawingMode = true;
    m_overlayWidget->setDrawingMode(true);

    m_startDrawingButton->setEnabled(false);
    m_stopDrawingButton->setEnabled(true);

    m_statusLabel->setText("그리기 모드 활성화 - 마우스로 선을 그어주세요");
    addLogMessage("그리기 모드가 활성화되었습니다.", "ACTION");
    updateButtonStates();
}

void LineDrawingDialog::onStopDrawingClicked()
{
    m_isDrawingMode = false;
    m_overlayWidget->setDrawingMode(false);

    m_startDrawingButton->setEnabled(true);
    m_stopDrawingButton->setEnabled(false);

    m_statusLabel->setText("그리기 모드 비활성화");
    addLogMessage("그리기 모드가 비활성화되었습니다.", "ACTION");
    updateButtonStates();
}

void LineDrawingDialog::onClearLinesClicked()
{
    int lineCount = m_overlayWidget->getLines().size();
    m_overlayWidget->clearLines();
    m_drawnLines.clear();
    m_statusLabel->setText("모든 선이 지워졌습니다");
    addLogMessage(QString("%1개의 선이 지워졌습니다.").arg(lineCount), "ACTION");
    updateButtonStates();
}

void LineDrawingDialog::onSendCoordinatesClicked()
{
    QList<QPair<QPoint, QPoint>> lines = m_overlayWidget->getLines();

    if (lines.isEmpty()) {
        addLogMessage("전송할 선이 없습니다.", "WARNING");
        QMessageBox::information(this, "알림", "전송할 선이 없습니다. 먼저 선을 그려주세요.");
        return;
    }

    addLogMessage(QString("좌표 전송을 시작합니다. (%1개 선)").arg(lines.size()), "INFO");

    // 모든 선의 좌표를 한 번에 전송
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        emit lineCoordinatesReady(line.first.x(), line.first.y(), line.second.x(), line.second.y());
        addLogMessage(QString("선 %1: (%2,%3) → (%4,%5)")
                          .arg(i + 1)
                          .arg(line.first.x()).arg(line.first.y())
                          .arg(line.second.x()).arg(line.second.y()), "COORD");
    }

    m_statusLabel->setText(QString("%1개의 선 좌표가 전송되었습니다").arg(lines.size()));
    addLogMessage(QString("총 %1개 선의 좌표 전송이 완료되었습니다.").arg(lines.size()), "SUCCESS");

    // 한 번만 알림창 표시
    QMessageBox::information(this, "전송 완료",
                             QString("%1개의 기준선 좌표가 서버로 전송되었습니다.").arg(lines.size()));

    // 전송 후 다이얼로그 닫기
    accept();
}

void LineDrawingDialog::onLineDrawn(const QPoint &start, const QPoint &end)
{
    m_drawnLines.append(qMakePair(start, end));
    m_statusLabel->setText(QString("선 그리기 완료 (총 %1개)").arg(m_drawnLines.size()));
    addLogMessage(QString("새 선이 그려졌습니다: (%1,%2) → (%3,%4)")
                      .arg(start.x()).arg(start.y())
                      .arg(end.x()).arg(end.y()), "DRAW");
    updateButtonStates();
}

void LineDrawingDialog::onPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    qDebug() << "플레이어 상태 변경:" << state;

    switch (state) {
    case QMediaPlayer::PlayingState:
        m_statusLabel->setText("비디오 스트림 재생 중");
        addLogMessage("비디오 스트림 재생이 시작되었습니다.", "STREAM");
        break;
    case QMediaPlayer::PausedState:
        m_statusLabel->setText("비디오 스트림 일시정지");
        addLogMessage("비디오 스트림이 일시정지되었습니다.", "STREAM");
        break;
    case QMediaPlayer::StoppedState:
        m_statusLabel->setText("비디오 스트림 중지됨");
        addLogMessage("비디오 스트림이 중지되었습니다.", "STREAM");
        break;
    }
    updateButtonStates();
}

void LineDrawingDialog::onPlayerError(QMediaPlayer::Error error, const QString &errorString)
{
    qDebug() << "미디어 플레이어 오류:" << error << errorString;

    m_statusLabel->setText(QString("오류: %1").arg(errorString));
    addLogMessage(QString("스트림 오류: %1").arg(errorString), "ERROR");
    QMessageBox::critical(this, "비디오 스트림 오류",
                          QString("비디오 스트림을 재생하는 중 오류가 발생했습니다:\n%1").arg(errorString));
}

void LineDrawingDialog::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "미디어 상태 변경:" << status;

    switch (status) {
    case QMediaPlayer::NoMedia:
        m_statusLabel->setText("미디어 없음");
        break;
    case QMediaPlayer::LoadingMedia:
        m_statusLabel->setText("미디어 로딩 중...");
        addLogMessage("미디어를 로딩하고 있습니다...", "STREAM");
        break;
    case QMediaPlayer::LoadedMedia:
        m_statusLabel->setText("미디어 로드됨");
        addLogMessage("미디어 로드가 완료되었습니다.", "STREAM");
        break;
    case QMediaPlayer::StalledMedia:
        m_statusLabel->setText("미디어 버퍼링 중...");
        addLogMessage("미디어 버퍼링 중입니다...", "STREAM");
        break;
    case QMediaPlayer::BufferingMedia:
        m_statusLabel->setText("미디어 버퍼링 중...");
        break;
    case QMediaPlayer::BufferedMedia:
        m_statusLabel->setText("미디어 재생 준비 완료");
        addLogMessage("미디어 재생 준비가 완료되었습니다.", "STREAM");
        break;
    case QMediaPlayer::EndOfMedia:
        m_statusLabel->setText("미디어 재생 종료");
        addLogMessage("미디어 재생이 종료되었습니다.", "STREAM");
        break;
    case QMediaPlayer::InvalidMedia:
        m_statusLabel->setText("잘못된 미디어");
        addLogMessage("잘못된 미디어입니다. URL을 확인해주세요.", "ERROR");
        QMessageBox::warning(this, "잘못된 미디어", "비디오 스트림을 재생할 수 없습니다. URL을 확인해주세요.");
        break;
    }
}

void LineDrawingDialog::updateFrameCount()
{
    if (m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_frameCount++;
        m_frameCountLabel->setText(QString("프레임: %1").arg(m_frameCount));
    }
}

void LineDrawingDialog::updateButtonStates()
{
    bool isStreaming = (m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState);
    bool hasLines = !m_overlayWidget->getLines().isEmpty();

    m_startDrawingButton->setEnabled(isStreaming && !m_isDrawingMode);
    m_stopDrawingButton->setEnabled(isStreaming && m_isDrawingMode);
    m_clearLinesButton->setEnabled(hasLines);
    m_sendCoordinatesButton->setEnabled(hasLines);
}

void LineDrawingDialog::addLogMessage(const QString &message, const QString &type)
{
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString colorCode;
    QString prefix;

    // 로그 타입별 색상과 접두사 설정
    if (type == "ERROR") {
        colorCode = "#dc3545";
        prefix = "❌";
    } else if (type == "WARNING") {
        colorCode = "#ffc107";
        prefix = "⚠️";
    } else if (type == "SUCCESS") {
        colorCode = "#28a745";
        prefix = "✅";
    } else if (type == "ACTION") {
        colorCode = "#007bff";
        prefix = "🔧";
    } else if (type == "DRAW") {
        colorCode = "#6f42c1";
        prefix = "✏️";
    } else if (type == "COORD") {
        colorCode = "#fd7e14";
        prefix = "📍";
    } else if (type == "STREAM") {
        colorCode = "#20c997";
        prefix = "📺";
    } else if (type == "SYSTEM") {
        colorCode = "#6c757d";
        prefix = "⚙️";
    } else {
        colorCode = "#333333";
        prefix = "ℹ️";
    }

    QString formattedMessage = QString(
                                   "<span style='color: %1;'><b>[%2]</b> %3 <span style='color: #666;'>%4</span> - %5</span>")
                                   .arg(colorCode)
                                   .arg(timestamp)
                                   .arg(prefix)
                                   .arg(type)
                                   .arg(message);

    m_logTextEdit->append(formattedMessage);

    // 자동 스크롤
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);

    // 로그 카운트 업데이트
    static int logCount = 0;
    logCount++;
    m_logCountLabel->setText(QString("로그: %1개").arg(logCount));

    // 최대 1000줄 제한
    QTextDocument *doc = m_logTextEdit->document();
    if (doc->blockCount() > 1000) {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, doc->blockCount() - 1000);
        cursor.removeSelectedText();
    }
}

void LineDrawingDialog::clearLog()
{
    m_logTextEdit->clear();
    m_logCountLabel->setText("로그: 0개");
    addLogMessage("로그가 지워졌습니다.", "SYSTEM");
}

void LineDrawingDialog::onClearLogClicked()
{
    clearLog();
}

void LineDrawingDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    // 다이얼로그 크기가 변경될 때 오버레이 위젯 위치도 업데이트
    if (m_overlayWidget && m_videoWidget) {
        QTimer::singleShot(0, [this]() {
            m_overlayWidget->setGeometry(m_videoWidget->geometry());
        });
    }
}
