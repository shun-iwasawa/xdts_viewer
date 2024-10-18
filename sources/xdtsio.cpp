#include "xdtsio.h"

#include <iostream>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QFile>
#include <QJsonDocument>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

QString XdtsFrameDataItem::SYMBOL_TICK_1    = "SYMBOL_TICK_1";
QString XdtsFrameDataItem::SYMBOL_TICK_2    = "SYMBOL_TICK_2";
QString XdtsFrameDataItem::SYMBOL_NULL_CELL = "SYMBOL_NULL_CELL";
QString XdtsFrameDataItem::SYMBOL_HYPHEN    = "SYMBOL_HYPHEN";

namespace {
static QByteArray identifierStr("exchangeDigitalTimeSheet Save Data");

int _tick1Id          = -1;
int _tick2Id          = -1;
bool _exportAllColumn = true;
}  // namespace
//-----------------------------------------------------------------------------
void XdtsHeader::read(const QJsonObject& json) {
  QRegExp rx("\\d{1,4}");
  // TODO: We could check if the keys are valid
  // before attempting to read them with QJsonObject::contains(),
  // but we assume that they are.
  m_cut = json["cut"].toString();
  if (!rx.exactMatch(m_cut))  // TODO: should handle an error
    std::cout << "The XdtsHeader value \"cut\" does not match the pattern."
              << std::endl;
  m_scene = json["scene"].toString();
  if (!rx.exactMatch(m_scene))
    std::cout << "The XdtsHeader value \"scene\" does not match the pattern."
              << std::endl;
}

void XdtsHeader::write(QJsonObject& json) const {
  json["cut"]   = m_cut;
  json["scene"] = m_scene;
}

//-----------------------------------------------------------------------------

void XdtsFrameDataItem::read(const QJsonObject& json) {
  m_id                   = DataId(qRound(json["id"].toDouble()));
  QJsonArray valuesArray = json["values"].toArray();
  for (int vIndex = 0; vIndex < valuesArray.size(); ++vIndex) {
    m_values.append(valuesArray[vIndex].toString());
  }
  if (json.contains("options")) {
    QJsonArray optionsArray = json["options"].toArray();
    for (int vIndex = 0; vIndex < optionsArray.size(); ++vIndex) {
      m_options.append(optionsArray[vIndex].toString());
    }
  }
}

void XdtsFrameDataItem::write(QJsonObject& json) const {
  json["id"] = int(m_id);
  QJsonArray valuesArray;
  foreach (const QString& value, m_values) {
    valuesArray.append(value);
  }
  json["values"] = valuesArray;

  if (!m_options.isEmpty()) {
    QJsonArray optionsArray;
    foreach (const QString& option, m_options) {
      optionsArray.append(option);
    }
    json["options"] = optionsArray;
  }
}

FrameData XdtsFrameDataItem::getFrameData() const {
  // int getCellNumber() const {
  if (m_values.isEmpty())
    return {QString(), FrameOption_None};  // EMPTY
                                           // if (m_values.isEmpty()) return 0;
  QString val = m_values.at(0);

  FrameOptionId option = FrameOption_None;
  if (!m_options.isEmpty()) {
    QString optionStr = m_options.at(0);
    if (optionStr == "OPTION_KEYFRAME")
      option = FrameOption_KeyFrame;
    else if (optionStr == "OPTION_REFERENCEFRAME")
      option = FrameOption_ReferenceFrame;
  }

  /*
  if (val == "SYMBOL_NULL_CELL")
    return TFrameId(-1);  // EMPTY
                          // ignore sheet symbols for now
  else if (val == "SYMBOL_HYPHEN")
    return TFrameId(-2);  // IGNORE
  else if (val == "SYMBOL_TICK_1")
    return TFrameId(SYMBOL_TICK_1);
  else if (val == "SYMBOL_TICK_2")
    return TFrameId(SYMBOL_TICK_2);
    */
  // return cell number
  return {val, option};
}

//-----------------------------------------------------------------------------
void XdtsTrackFrameItem::read(const QJsonObject& json) {
  QJsonArray dataArray = json["data"].toArray();
  for (int dataIndex = 0; dataIndex < dataArray.size(); ++dataIndex) {
    QJsonObject dataObject = dataArray[dataIndex].toObject();
    XdtsFrameDataItem data;
    data.read(dataObject);
    m_data.append(data);
  }
  m_frame = json["frame"].toInt();
}

void XdtsTrackFrameItem::write(QJsonObject& json) const {
  QJsonArray dataArray;
  foreach (const XdtsFrameDataItem& data, m_data) {
    QJsonObject dataObject;
    data.write(dataObject);
    dataArray.append(dataObject);
  }
  json["data"] = dataArray;

  json["frame"] = m_frame;
}

//-----------------------------------------------------------------------------

QPair<int, FrameData> XdtsTrackFrameItem::frameFdata() const {
  return QPair<int, FrameData>(m_frame, m_data[0].getFrameData());
}

//-----------------------------------------------------------------------------
void XdtsFieldTrackItem::read(const QJsonObject& json) {
  QJsonArray frameArray = json["frames"].toArray();
  for (int frameIndex = 0; frameIndex < frameArray.size(); ++frameIndex) {
    QJsonObject frameObject = frameArray[frameIndex].toObject();
    XdtsTrackFrameItem frame;
    frame.read(frameObject);
    m_frames.append(frame);
  }
  m_trackNo = json["trackNo"].toInt();
}

void XdtsFieldTrackItem::write(QJsonObject& json) const {
  QJsonArray frameArray;
  foreach (const XdtsTrackFrameItem& frame, m_frames) {
    QJsonObject frameObject;
    frame.write(frameObject);
    frameArray.append(frameObject);
  }
  json["frames"] = frameArray;

  json["trackNo"] = m_trackNo;
}
//-----------------------------------------------------------------------------

static bool frameLessThan(const QPair<int, FrameData>& v1,
                          const QPair<int, FrameData>& v2) {
  return v1.first < v2.first;
}

QVector<FrameData> XdtsFieldTrackItem::getCellFrameDataTrack(
    QList<int>& tick1, QList<int>& tick2, bool& isHoldFrame) const {
  QList<QPair<int, FrameData>> frameFdataList;
  for (const XdtsTrackFrameItem& frame : m_frames)
    frameFdataList.append(frame.frameFdata());
  std::sort(frameFdataList.begin(), frameFdataList.end(), frameLessThan);

  QVector<FrameData> cells;
  int currentFrame        = 0;
  FrameData initialNumber = {QString(), FrameOption_None};
  for (QPair<int, FrameData>& frameFdata : frameFdataList) {
    while (currentFrame < frameFdata.first) {
      cells.append({QString(), FrameOption_None});
      // cells.append((cells.isEmpty()) ? initialNumber : cells.last());
      currentFrame++;
    }
    // CSP may export negative frame data (although it is not allowed in XDTS
    // format specification) so handle such case.
    if (frameFdata.first < 0) {
      initialNumber = frameFdata.second;
      continue;
    }

    FrameData cellFdata = frameFdata.second;
    if (cellFdata.frame == XdtsFrameDataItem::SYMBOL_HYPHEN) {  // IGNORE case
      cells.append({QString(), FrameOption_None});
      // cells.append((cells.isEmpty()) ? QString() : cells.last());
    } else if (cellFdata.frame ==
               XdtsFrameDataItem::SYMBOL_TICK_1) {  // SYMBOL_TICK_1
      cells.append(cellFdata);
      // cells.append((cells.isEmpty()) ? QString() : cells.last());
      tick1.append(currentFrame);
    } else if (cellFdata.frame ==
               XdtsFrameDataItem::SYMBOL_TICK_2) {  // SYMBOL_TICK_2
      cells.append(cellFdata);
      // cells.append((cells.isEmpty()) ? QString() : cells.last());
      tick2.append(currentFrame);
    } else
      cells.append(cellFdata);
    currentFrame++;
  }

  if (frameFdataList.size() == 2 && frameFdataList[0].first == 0 &&
      !frameFdataList[0].second.frame.startsWith("SYMBOL_") &&
      frameFdataList[1].second.frame == XdtsFrameDataItem::SYMBOL_NULL_CELL)
    isHoldFrame = true;

  return cells;
}

//-----------------------------------------------------------------------------

void XdtsTimeTableFieldItem::read(const QJsonObject& json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  QJsonArray trackArray = json["tracks"].toArray();
  for (int trackIndex = 0; trackIndex < trackArray.size(); ++trackIndex) {
    QJsonObject trackObject = trackArray[trackIndex].toObject();
    XdtsFieldTrackItem track;
    track.read(trackObject);
    m_tracks.append(track);
  }
}

void XdtsTimeTableFieldItem::write(QJsonObject& json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray trackArray;
  foreach (const XdtsFieldTrackItem& track, m_tracks) {
    QJsonObject trackObject;
    track.write(trackObject);
    trackArray.append(trackObject);
  }
  json["tracks"] = trackArray;
}

QList<int> XdtsTimeTableFieldItem::getOccupiedColumns() const {
  QList<int> ret;
  for (const XdtsFieldTrackItem& track : m_tracks) {
    if (!track.isEmpty()) ret.append(track.getTrackNo());
  }
  return ret;
}

QVector<FrameData> XdtsTimeTableFieldItem::getColumnTrack(
    int col, QList<int>& tick1, QList<int>& tick2, bool& isHoldFrame) const {
  for (const XdtsFieldTrackItem& track : m_tracks) {
    if (track.getTrackNo() != col) continue;
    return track.getCellFrameDataTrack(tick1, tick2, isHoldFrame);
  }
  return QVector<FrameData>();
}

//-----------------------------------------------------------------------------

void XdtsTimeTableHeaderItem::read(const QJsonObject& json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  QJsonArray namesArray = json["names"].toArray();
  for (int nIndex = 0; nIndex < namesArray.size(); ++nIndex) {
    m_names.append(namesArray[nIndex].toString());
  }
}

void XdtsTimeTableHeaderItem::write(QJsonObject& json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray namesArray;
  foreach (const QString name, m_names) {
    namesArray.append(name);
  }
  json["names"] = namesArray;
}

//-----------------------------------------------------------------------------

void XdtsTimeTableItem::read(const QJsonObject& json) {
  if (json.contains("fields")) {
    QJsonArray fieldArray = json["fields"].toArray();
    for (int fieldIndex = 0; fieldIndex < fieldArray.size(); ++fieldIndex) {
      QJsonObject fieldObject = fieldArray[fieldIndex].toObject();
      XdtsTimeTableFieldItem field;
      field.read(fieldObject);
      m_fields.append(field);
      if (field.isCellField()) m_cellFieldIndex = fieldIndex;
    }
  }
  m_duration             = json["duration"].toInt();
  m_name                 = json["name"].toString();
  QJsonArray headerArray = json["timeTableHeaders"].toArray();
  for (int headerIndex = 0; headerIndex < headerArray.size(); ++headerIndex) {
    QJsonObject headerObject = headerArray[headerIndex].toObject();
    XdtsTimeTableHeaderItem header;
    header.read(headerObject);
    m_timeTableHeaders.append(header);
    if (header.isCellField()) m_cellHeaderIndex = headerIndex;
  }
}

void XdtsTimeTableItem::write(QJsonObject& json) const {
  if (!m_fields.isEmpty()) {
    QJsonArray fieldArray;
    foreach (const XdtsTimeTableFieldItem& field, m_fields) {
      QJsonObject fieldObject;
      field.write(fieldObject);
      fieldArray.append(fieldObject);
    }
    json["fields"] = fieldArray;
  }
  json["duration"] = m_duration;
  json["name"]     = m_name;
  QJsonArray headerArray;
  foreach (const XdtsTimeTableHeaderItem header, m_timeTableHeaders) {
    QJsonObject headerObject;
    header.write(headerObject);
    headerArray.append(headerObject);
  }
  json["timeTableHeaders"] = headerArray;
}

QStringList XdtsTimeTableItem::getLevelNames() const {
  // obtain column labels from the header
  assert(m_cellHeaderIndex >= 0);
  QStringList labels = m_timeTableHeaders.at(m_cellHeaderIndex).getLayerNames();
  // obtain occupied column numbers in the field
  assert(m_cellFieldIndex >= 0);
  QList<int> occupiedColumns =
      m_fields.at(m_cellFieldIndex).getOccupiedColumns();
  // return the names of occupied columns
  QStringList ret;
  for (const int columnId : occupiedColumns) ret.append(labels.at(columnId));
  return ret;
}

//-----------------------------------------------------------------------------

void XdtsData::read(const QJsonObject& json) {
  if (json.contains("header")) {
    QJsonObject headerObject = json["header"].toObject();
    m_header.read(headerObject);
  }
  QJsonArray tableArray = json["timeTables"].toArray();
  for (int tableIndex = 0; tableIndex < tableArray.size(); ++tableIndex) {
    QJsonObject tableObject = tableArray[tableIndex].toObject();
    XdtsTimeTableItem table;
    table.read(tableObject);
    m_timeTables.append(table);
  }
  m_version = Version(qRound(json["version"].toDouble()));
}

void XdtsData::write(QJsonObject& json) const {
  if (!m_header.isEmpty()) {
    QJsonObject headerObject;
    m_header.write(headerObject);
    json["header"] = headerObject;
  }
  QJsonArray tableArray;
  foreach (const XdtsTimeTableItem& table, m_timeTables) {
    QJsonObject tableObject;
    table.write(tableObject);
    tableArray.append(tableObject);
  }
  json["timeTables"] = tableArray;
  json["version"]    = int(m_version);
}

QStringList XdtsData::getLevelNames() const {
  // currently support only the first page of time tables
  return m_timeTables.at(0).getLevelNames();
}

//-----------------------------------------------------------------------------

bool loadXdtsScene(XdtsData* data, const QString& scenePath) {
  QApplication::restoreOverrideCursor();
  // read the Json file
  QFile loadFile(scenePath);
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  QByteArray dataArray = loadFile.readAll();

  if (!dataArray.startsWith(identifierStr)) {
    qWarning("The first line does not start with XDTS identifier string.");
    return false;
  }
  // remove identifier
  dataArray.remove(0, identifierStr.length());

  QJsonDocument loadDoc(QJsonDocument::fromJson(dataArray));

  // XdtsData xdtsData;
  // xdtsData.read(loadDoc.object());
  data->read(loadDoc.object());

  /*
  // obtain level names
  QStringList levelNames = xdtsData.getLevelNames();
  // in case multiple columns have the same name
  levelNames.removeDuplicates();

  XdtsTimeTableFieldItem cellField = xdtsData.timeTable().getCellField();
  XdtsTimeTableHeaderItem cellHeader = xdtsData.timeTable().getCellHeader();
  int duration = xdtsData.timeTable().getDuration();
  QStringList layerNames = cellHeader.getLayerNames();
  QList<int> columns = cellField.getOccupiedColumns();
  for (int column : columns) {
    QString levelName = layerNames.at(column);
    TXshLevel* level = levels.value(levelName);
    TXshSimpleLevel* sl = level->getSimpleLevel();
    QList<int> tick1, tick2;
    QVector<TFrameId> track = cellField.getColumnTrack(column, tick1, tick2);

    int row = 0;
    std::vector<TFrameId>::iterator it;
    for (TFrameId fid : track) {
      if (fid.getNumber() == -1)  // EMPTY cell case
        row++;
      else {
        // modify frameId to be with the same frame format as existing frames
        if (sl) sl->formatFId(fid, tmplFId);
        xsh->setCell(row++, column, TXshCell(level, fid));
      }
    }
    // if the last cell is not "SYMBOL_NULL_CELL", continue the cell
    // to the end of the sheet
    TFrameId lastFid = track.last();
    if (lastFid.getNumber() != -1) {
      // modify frameId to be with the same frame format as existing frames
      if (sl) sl->formatFId(lastFid, tmplFId);
      for (; row < duration; row++)
        xsh->setCell(row, column, TXshCell(level, TFrameId(lastFid)));
    }

    // set cell marks
    TXshCellColumn* cellColumn = xsh->getColumn(column)->getCellColumn();
    if (tick1Id >= 0) {
      for (auto tick1f : tick1) cellColumn->setCellMark(tick1f, tick1Id);
    }
    if (tick2Id >= 0) {
      for (auto tick2f : tick2) cellColumn->setCellMark(tick2f, tick2Id);
    }

    TStageObject* pegbar =
      xsh->getStageObject(TStageObjectId::ColumnId(column));
    if (pegbar) pegbar->setName(levelName.toStdString());
  }
  xsh->updateFrameCount();

  // if the duration is shorter than frame count, then set it both in
  // preview range and output range.
  if (duration < xsh->getFrameCount()) {
    scene->getProperties()->getPreviewProperties()->setRange(0, duration - 1,
      1);
    scene->getProperties()->getOutputProperties()->setRange(0, duration - 1, 1);
  }

  // emit signal here for updating the frame slider range of flip console
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  */
  return true;
}
