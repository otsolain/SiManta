#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QLayout>
#include <QStyle>
#include <QMap>
#include <QLabel>
#include <QPixmap>

#include "student_tile.h"
#include "protocol.h"

namespace LabMonitor {

/**
 * FlowLayout — Custom layout that arranges widgets in a flowing grid,
 * automatically wrapping to the next row when the width is exceeded.
 * This provides responsive grid behavior like CSS flexbox wrap.
 */
class FlowLayout : public QLayout
{
    Q_OBJECT

public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int  count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int  heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect& rect) override;

    int horizontalSpacing() const;
    int verticalSpacing() const;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> m_itemList;
    int m_hSpace;
    int m_vSpace;
};

/**
 * StudentGrid — Scrollable area containing student tiles in a flow layout.
 * Includes a tab header showing "All (N)" count and an empty state message.
 */
class StudentGrid : public QWidget
{
    Q_OBJECT

public:
    explicit StudentGrid(QWidget* parent = nullptr);

    // Add/remove students
    StudentTile* addStudent(const StudentInfo& info);
    void removeStudent(const QString& studentId);

    // Update a student's screenshot
    void updateScreenshot(const QString& studentId, const QPixmap& pixmap);

    // Update student online status
    void setStudentOnline(const QString& studentId, bool online);

    // Get a tile
    StudentTile* getTile(const QString& studentId) const;

    // Get all tiles
    QList<StudentTile*> allTiles() const;

    // Selection
    void selectAll();
    void deselectAll();
    QList<StudentTile*> selectedTiles() const;

    // Thumbnail size
    void setThumbnailSize(int size);

    // Count
    int studentCount() const { return m_tiles.size(); }
    StudentTile* tileById(const QString& id) const { return m_tiles.value(id, nullptr); }

signals:
    void studentClicked(const QString& studentId);
    void studentDoubleClicked(const QString& studentId);
    void selectionChanged();

private slots:
    void onTileClicked(const QString& studentId);

private:
    void updateTabHeader();
    void updateEmptyState();

    QLabel*       m_tabHeader = nullptr;
    QScrollArea*  m_scrollArea = nullptr;
    QWidget*      m_gridContainer = nullptr;
    FlowLayout*   m_flowLayout = nullptr;
    QLabel*       m_emptyLabel = nullptr;

    QMap<QString, StudentTile*> m_tiles;
    int m_thumbWidth = 240;
};

} // namespace LabMonitor
