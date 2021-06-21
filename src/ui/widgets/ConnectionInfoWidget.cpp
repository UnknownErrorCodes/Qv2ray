#include "ConnectionInfoWidget.hpp"

#include "Common/ProfileHelpers.hpp"
#include "Common/Utils.hpp"
#include "Profile/KernelManager.hpp"
#include "Profile/ProfileManager.hpp"
#include "QRCodeHelper/QRCodeHelper.hpp"
#include "ui/WidgetUIBase.hpp"

constexpr auto INDEX_CONNECTION = 0;
constexpr auto INDEX_GROUP = 1;

QvMessageBusSlotImpl(ConnectionInfoWidget)
{
    switch (msg)
    {
        MBRetranslateDefaultImpl;
        MBUpdateColorSchemeDefaultImpl;
        case HIDE_WINDOWS:
        case SHOW_WINDOWS: break;
    }
}

void ConnectionInfoWidget::updateColorScheme()
{
    latencyBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("ping_gauge")));
    deleteBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("ashbin")));
    editBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("edit")));
    editJsonBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("code")));
    shareLinkTxt->setStyleSheet("border-bottom: 1px solid gray; border-radius: 0px; padding: 2px; background-color: " +
                                this->palette().color(this->backgroundRole()).name(QColor::HexRgb));
    groupSubsLinkTxt->setStyleSheet("border-bottom: 1px solid gray; border-radius: 0px; padding: 2px; background-color: " +
                                    this->palette().color(this->backgroundRole()).name(QColor::HexRgb));

#pragma message("TODO: Darkmode should be in StyleManager")
    auto isDarkTheme = GlobalConfig->appearanceConfig->DarkModeUI;
    qrPixmapBlured = BlurImage(ColorizeImage(qrPixmap, isDarkTheme ? Qt::black : Qt::white, 0.7), 35);
    qrLabel->setPixmap(IsComplexConfig(connectionId) ? QPixmap(QStringLiteral(":/assets/icons/qv2ray.png")) : (isRealPixmapShown ? qrPixmap : qrPixmapBlured));
    const auto isCurrentItem = QvBaselib->KernelManager()->CurrentConnection().connectionId == connectionId;
    connectBtn->setIcon(QIcon(isCurrentItem ? QV2RAY_COLORSCHEME_FILE("stop") : QV2RAY_COLORSCHEME_FILE("start")));
}

ConnectionInfoWidget::ConnectionInfoWidget(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    //
    QvMessageBusConnect();
    updateColorScheme();
    //
    shareLinkTxt->setAutoFillBackground(true);
    shareLinkTxt->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
    shareLinkTxt->installEventFilter(this);
    groupSubsLinkTxt->installEventFilter(this);
    qrLabel->installEventFilter(this);
    //
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnConnected, this, &ConnectionInfoWidget::OnConnected);
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnDisconnected, this, &ConnectionInfoWidget::OnDisConnected);
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnGroupRenamed, this, &ConnectionInfoWidget::OnGroupRenamed);
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnConnectionModified, this, &ConnectionInfoWidget::OnConnectionModified);
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnConnectionLinkedWithGroup, this, &ConnectionInfoWidget::OnConnectionModified_Pair);
    connect(QvBaselib->ProfileManager(), &Qv2rayBase::Profile::ProfileManager::OnConnectionRemovedFromGroup, this, &ConnectionInfoWidget::OnConnectionModified_Pair);
}

void ConnectionInfoWidget::ShowDetails(const ConnectionGroupPair &idpair)
{
    this->groupId = idpair.groupId;
    this->connectionId = idpair.connectionId;
    bool isConnection = !connectionId.isNull();
    //
    editBtn->setEnabled(isConnection);
    editJsonBtn->setEnabled(isConnection);
    connectBtn->setEnabled(isConnection);
    stackedWidget->setCurrentIndex(isConnection ? INDEX_CONNECTION : INDEX_GROUP);
    if (isConnection)
    {
        auto shareLink = ConvertConfigToString(idpair.connectionId);
        //
        shareLinkTxt->setText(shareLink);
        protocolLabel->setText(GetConnectionProtocolDescription(connectionId));
        //
        groupLabel->setText(GetDisplayName(groupId));

        const auto root = QvBaselib->ProfileManager()->GetConnection(connectionId);
        if (!root.outbounds.isEmpty())
        {
            auto [protocol, host, port] = GetOutboundInfoTuple(root.outbounds.first());
            Q_UNUSED(protocol)
            addressLabel->setText(host);
            portLabel->setNum(port);
        }

        shareLinkTxt->setCursorPosition(0);

#pragma message("TODO: Darkmode should be in StyleManager")
        auto isDarkTheme = GlobalConfig->appearanceConfig->DarkModeUI;
        qrPixmap = QPixmap::fromImage(EncodeQRCode(shareLink, qrLabel->width() * devicePixelRatio()));
        //
        qrPixmapBlured = BlurImage(ColorizeImage(qrPixmap, isDarkTheme ? QColor(Qt::black) : QColor(Qt::white), 0.7), 35);
        //
        isRealPixmapShown = false;
        qrLabel->setPixmap(IsComplexConfig(connectionId) ? QPixmap(QStringLiteral(":/assets/icons/qv2ray.png")) : qrPixmapBlured);
        qrLabel->setScaledContents(true);
        const auto isCurrentItem = QvBaselib->KernelManager()->CurrentConnection().connectionId == connectionId;
        connectBtn->setIcon(QIcon(isCurrentItem ? QV2RAY_COLORSCHEME_FILE("stop") : QV2RAY_COLORSCHEME_FILE("start")));
    }
    else
    {
        connectBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("start")));
        groupNameLabel->setText(GetDisplayName(groupId));

        QString shareLinks;
        for (const auto &connection : QvBaselib->ProfileManager()->GetConnections(groupId))
        {
            const auto link = ConvertConfigToString(connection);
            shareLinks.append("\n" + link);
        }

        groupShareTxt->setPlainText(shareLinks);
        const auto &groupMetaData = QvBaselib->ProfileManager()->GetGroupObject(groupId);
        groupSubsLinkTxt->setText(groupMetaData.subscription_config.isSubscription ? groupMetaData.subscription_config.address : tr("Not a subscription"));
    }
}

ConnectionInfoWidget::~ConnectionInfoWidget()
{
}

void ConnectionInfoWidget::OnConnectionModified(const ConnectionId &id)
{
    if (id == this->connectionId)
        ShowDetails({ id, groupId });
}

void ConnectionInfoWidget::OnConnectionModified_Pair(const ConnectionGroupPair &id)
{
    if (id.connectionId == this->connectionId && id.groupId == this->groupId)
        ShowDetails(id);
}
void ConnectionInfoWidget::OnGroupRenamed(const GroupId &id, const QString &oldName, const QString &newName)
{
    Q_UNUSED(oldName)
    if (this->groupId == id)
    {
        groupNameLabel->setText(newName);
        groupLabel->setText(newName);
    }
}

void ConnectionInfoWidget::on_connectBtn_clicked()
{
    if (QvBaselib->ProfileManager()->IsConnected({ connectionId, groupId }))
    {
        QvBaselib->ProfileManager()->StopConnection();
    }
    else
    {
        QvBaselib->ProfileManager()->StartConnection({ connectionId, groupId });
    }
}

void ConnectionInfoWidget::on_editBtn_clicked()
{
    emit OnEditRequested(connectionId);
}

void ConnectionInfoWidget::on_editJsonBtn_clicked()
{
    emit OnJsonEditRequested(connectionId);
}

void ConnectionInfoWidget::on_deleteBtn_clicked()
{
    if (QvBaselib->Ask(tr("Delete an item"), tr("Are you sure to delete the current item?")) == Qv2rayBase::MessageOpt::Yes)
    {
        if (!connectionId.isNull())
        {
            QvBaselib->ProfileManager()->RemoveFromGroup(connectionId, groupId);
        }
        else
        {
            QvBaselib->ProfileManager()->DeleteGroup(groupId);
        }
    }
}

bool ConnectionInfoWidget::eventFilter(QObject *object, QEvent *event)
{
    if (shareLinkTxt->underMouse() && event->type() == QEvent::MouseButtonRelease)
    {
        if (!shareLinkTxt->hasSelectedText())
            shareLinkTxt->selectAll();
    }
    else if (groupSubsLinkTxt->underMouse() && event->type() == QEvent::MouseButtonRelease)
    {
        if (!groupSubsLinkTxt->hasSelectedText())
            groupSubsLinkTxt->selectAll();
    }
    else if (qrLabel->underMouse() && event->type() == QEvent::MouseButtonRelease)
    {
        qrLabel->setPixmap(IsComplexConfig(connectionId) ? QPixmap(":/assets/icons/qv2ray.png") : (isRealPixmapShown ? qrPixmapBlured : qrPixmap));
        isRealPixmapShown = !isRealPixmapShown;
    }

    return QWidget::eventFilter(object, event);
}

void ConnectionInfoWidget::OnConnected(const ConnectionGroupPair &id)
{
    if (id == ConnectionGroupPair{ connectionId, groupId })
    {
        connectBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("stop")));
    }
}

void ConnectionInfoWidget::OnDisConnected(const ConnectionGroupPair &id)
{
    if (id == ConnectionGroupPair{ connectionId, groupId })
    {
        connectBtn->setIcon(QIcon(QV2RAY_COLORSCHEME_FILE("start")));
    }
}

void ConnectionInfoWidget::on_latencyBtn_clicked()
{
    if (!connectionId.isNull())
    {
        QvBaselib->ProfileManager()->StartLatencyTest(connectionId, GlobalConfig->behaviorConfig->DefaultLatencyTestEngine);
    }
    else
    {
        QvBaselib->ProfileManager()->StartLatencyTest(groupId, GlobalConfig->behaviorConfig->DefaultLatencyTestEngine);
    }
}
