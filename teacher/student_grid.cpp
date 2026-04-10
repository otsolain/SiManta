#include "student_grid.h"
#include "styles.h"

#include <QStyle>
#include <QWidget>
#include <QVBoxLayout>
#include <QApplication>

namespace LabMonitor {

// ════════════════════════════════════════════════════════════
// FlowLayout Implementation
// ════════════════════════════════════════════════════════════

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    QLayoutItem* item;
    while ((item = takeAt(0)))
        delete item;
}

void FlowLayout::addItem(QLayoutItem* item) { m_itemList.append(item); }
int  FlowLayout::count() const { return m_itemList.size(); }
QLayoutItem* FlowLayout::itemAt(int index) const { return m_itemList.value(index); }

QLayoutItem* FlowLayout::takeAt(int index)
{
    if (index >= 0 && index < m_itemList.size())
        return m_itemList.takeAt(index);
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }
bool FlowLayout::hasHeightForWidth() const { return true; }
int  FlowLayout::heightForWidth(int width) const { return doLayout(QRect(0, 0, width, 0), true); }
QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem* item : m_itemList)
        size = size.expandedTo(item->minimumSize());
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}
QSize FlowLayout::sizeHint() const { return minimumSize(); }
void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::horizontalSpacing() const
{
    return (m_hSpace >= 0) ? m_hSpace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    return (m_vSpace >= 0) ? m_vSpace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    int spaceX = horizontalSpacing();
    int spaceY = verticalSpacing();

    for (QLayoutItem* item : m_itemList) {
        if (!item->widget() || item->widget()->isHidden())
            continue;

        int nextX = x + item->sizeHint().width() + spaceX;
        if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly)
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));

        x = nextX;
        lineHeight = qMax(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject* parent = this->parent();
    if (!parent) return -1;
    if (parent->isWidgetType()) {
        auto* pw = static_cast<QWidget*>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout*>(parent)->spacing();
}

// ════════════════════════════════════════════════════════════
// StudentGrid Implementation
// ════════════════════════════════════════════════════════════

StudentGrid::StudentGrid(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tab header "All (0)"
    m_tabHeader = new QLabel("All (0)", this);
    auto* tabHeaderWidget = new QWidget(this);
    tabHeaderWidget->setObjectName("TabHeader");
    tabHeaderWidget->setAttribute(Qt::WA_StyledBackground, true);
    tabHeaderWidget->setAutoFillBackground(true);
    tabHeaderWidget->setStyleSheet(Styles::tabHeaderStyle());
    auto* tabLayout = new QHBoxLayout(tabHeaderWidget);
    tabLayout->setContentsMargins(12, 4, 12, 4);
    tabLayout->addWidget(m_tabHeader);
    tabLayout->addStretch();
    mainLayout->addWidget(tabHeaderWidget);

    // Scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(Styles::scrollAreaStyle());

    m_gridContainer = new QWidget();
    m_gridContainer->setStyleSheet(QStringLiteral("background: %1;").arg(Styles::Colors::MainBg));
    m_flowLayout = new FlowLayout(m_gridContainer, 16, 12, 12);
    m_gridContainer->setLayout(m_flowLayout);

    m_scrollArea->setWidget(m_gridContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // Empty state label
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setText("Waiting for students to connect...");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: %1;"
        "  font-family: %2;"
        "  font-size: %3pt;"
        "  font-style: italic;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 60px;"
        "}"
    ).arg(Styles::Colors::TextMuted,
          Styles::Fonts::Family,
          QString::number(Styles::Fonts::EmptyMessage)));

    mainLayout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_scrollArea->setVisible(false);

    updateEmptyState();
}

StudentTile* StudentGrid::addStudent(const StudentInfo& info)
{
    if (m_tiles.contains(info.id)) {
        return m_tiles[info.id];
    }

    auto* tile = new StudentTile(info, m_gridContainer);
    tile->setThumbnailSize(QSize(m_thumbWidth, static_cast<int>(m_thumbWidth * 0.667)));

    connect(tile, &StudentTile::clicked,
            this, &StudentGrid::onTileClicked);
    connect(tile, &StudentTile::doubleClicked,
            this, &StudentGrid::studentDoubleClicked);

    m_flowLayout->addWidget(tile);
    m_tiles[info.id] = tile;

    updateTabHeader();
    updateEmptyState();

    return tile;
}

void StudentGrid::removeStudent(const QString& studentId)
{
    if (!m_tiles.contains(studentId)) return;

    auto* tile = m_tiles.take(studentId);
    m_flowLayout->removeWidget(tile);
    tile->deleteLater();

    updateTabHeader();
    updateEmptyState();
}

void StudentGrid::updateScreenshot(const QString& studentId, const QPixmap& pixmap)
{
    if (auto* tile = m_tiles.value(studentId)) {
        tile->updateScreenshot(pixmap);
    }
}

void StudentGrid::setStudentOnline(const QString& studentId, bool online)
{
    if (auto* tile = m_tiles.value(studentId)) {
        tile->setOnline(online);
    }
}

StudentTile* StudentGrid::getTile(const QString& studentId) const
{
    return m_tiles.value(studentId);
}

QList<StudentTile*> StudentGrid::allTiles() const
{
    return m_tiles.values();
}

void StudentGrid::selectAll()
{
    for (auto* tile : m_tiles) {
        tile->setSelected(true);
    }
    emit selectionChanged();
}

void StudentGrid::deselectAll()
{
    for (auto* tile : m_tiles) {
        tile->setSelected(false);
    }
    emit selectionChanged();
}

QList<StudentTile*> StudentGrid::selectedTiles() const
{
    QList<StudentTile*> selected;
    for (auto* tile : m_tiles) {
        if (tile->isSelected()) {
            selected.append(tile);
        }
    }
    return selected;
}

void StudentGrid::setThumbnailSize(int size)
{
    m_thumbWidth = size;
    QSize thumbSize(size, static_cast<int>(size * 0.667));

    for (auto* tile : m_tiles) {
        tile->setThumbnailSize(thumbSize);
    }

    // Force layout recalculation
    m_gridContainer->updateGeometry();
}

void StudentGrid::onTileClicked(const QString& studentId)
{
    auto* clickedTile = m_tiles.value(studentId);
    if (!clickedTile) return;

    bool ctrl = qApp->keyboardModifiers() & Qt::ControlModifier;

    if (!ctrl) {
        // Deselect all others, keep clicked tile's current state
        bool clickedState = clickedTile->isSelected();
        for (auto* tile : m_tiles) {
            if (tile != clickedTile) {
                tile->setSelected(false);
            }
        }
        // Tile already toggled by mousePressEvent, don't change it
    }
    // With Ctrl: tile already toggled, nothing else to do

    emit studentClicked(studentId);
    emit selectionChanged();
}

void StudentGrid::updateTabHeader()
{
    int online = 0;
    for (auto* tile : m_tiles) {
        if (tile->isOnline()) online++;
    }
    m_tabHeader->setText(QStringLiteral("All (%1)").arg(m_tiles.size()));
}

void StudentGrid::updateEmptyState()
{
    bool empty = m_tiles.isEmpty();
    m_emptyLabel->setVisible(empty);
    m_scrollArea->setVisible(!empty);
}

} // namespace LabMonitor
