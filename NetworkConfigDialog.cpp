#include "NetworkConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QRegularExpressionValidator>
#include <QIntValidator>

NetworkConfigDialog::NetworkConfigDialog(QWidget *parent)
    : QDialog(parent)
    , m_rtspUrlEdit(nullptr)
    , m_tcpHostEdit(nullptr)
    , m_tcpPortEdit(nullptr)
{
    setupUI();
    setWindowTitle("네트워크 설정");
    setModal(true);
    setFixedSize(400, 300);
}

NetworkConfigDialog::~NetworkConfigDialog()
{
}

void NetworkConfigDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 제목
    QLabel *titleLabel = new QLabel("Network Connection Settings");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #F37321; padding: 5px;");
    titleLabel->setMaximumHeight(80);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // 폼 레이아웃
    QFormLayout *formLayout = new QFormLayout();
    
    // RTSP URL 입력
    m_rtspUrlEdit = new QLineEdit();
    m_rtspUrlEdit->setMinimumHeight(40);
    m_rtspUrlEdit->setPlaceholderText("예: rtsp://192.168.1.100:554/stream");
    m_rtspUrlEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }");
    formLayout->addRow("RTSP URL:", m_rtspUrlEdit);
    
    // TCP 호스트 입력
    m_tcpHostEdit = new QLineEdit();
    m_tcpHostEdit->setMinimumHeight(40);
    m_tcpHostEdit->setPlaceholderText("예: 192.168.1.100");
    m_tcpHostEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }");
    
    // IP 주소 유효성 검사
    QRegularExpression ipRegex("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    QRegularExpressionValidator *ipValidator = new QRegularExpressionValidator(ipRegex, this);
    m_tcpHostEdit->setValidator(ipValidator);
    
    formLayout->addRow("TCP 호스트:", m_tcpHostEdit);
    
    // TCP 포트 입력
    m_tcpPortEdit = new QLineEdit();
    m_tcpPortEdit->setMinimumHeight(35);
    m_tcpPortEdit->setPlaceholderText("예: 8080");
    m_tcpPortEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }");
    
    // 포트 번호 유효성 검사 (1-65535)
    QIntValidator *portValidator = new QIntValidator(1, 65535, this);
    m_tcpPortEdit->setValidator(portValidator);
    
    formLayout->addRow("TCP 포트:", m_tcpPortEdit);
    
    mainLayout->addLayout(formLayout);
    
    // 버튼 영역
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->setStyleSheet("QPushButton { padding: 8px 16px; border: none; border-radius: 4px; font-weight: bold; } "
                            "QPushButton[text='OK'] { background-color: #4CAF50; color: white; } "
                            "QPushButton[text='OK']:hover { background-color: #45a049; } "
                            "QPushButton[text='Cancel'] { background-color: #f44336; color: white; } "
                            "QPushButton[text='Cancel']:hover { background-color: #d32f2f; }");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
    
    setLayout(mainLayout);
}

QString NetworkConfigDialog::getRtspUrl() const
{
    return m_rtspUrlEdit->text().trimmed();
}

void NetworkConfigDialog::setRtspUrl(const QString &url)
{
    m_rtspUrlEdit->setText(url);
}

QString NetworkConfigDialog::getTcpHost() const
{
    return m_tcpHostEdit->text().trimmed();
}

void NetworkConfigDialog::setTcpHost(const QString &host)
{
    m_tcpHostEdit->setText(host);
}

int NetworkConfigDialog::getTcpPort() const
{
    return m_tcpPortEdit->text().toInt();
}

void NetworkConfigDialog::setTcpPort(int port)
{
    m_tcpPortEdit->setText(QString::number(port));
}
