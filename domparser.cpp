
#include "domparser.h"

#include <QRegExp>

QString strRootType;

DomParser::DomParser() {}
DomParser::~DomParser() {}

DomModel* DomParser::fromDom(QDomDocument d) {
  // get root element
  QDomElement root = d.documentElement();

  // check cosistency

  if (root.tagName() != "plist") {
    qWarning("The file is not a plist!");
    return NULL;
  }

  // get dict element
  QDomElement dict = root.firstChildElement();

  // create model
  DomModel* m = new DomModel();

  // create root item in model
  DomItem* item = m->getRoot()->addChild();
  item->setName("plist");
  item->setType(strRootType);

  // parse doc recursively
  parseElement(dict, item);

  return m;
}

QDomDocument DomParser::toDom(DomModel* m) {
  // create doc with doctype
  QDomDocument doc(
      "plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"");

  // create and add processing instruction
  QDomProcessingInstruction instr = doc.createProcessingInstruction(
      "xml", "version=\"1.0\" encoding=\"UTF-8\"");
  doc.appendChild(instr);

  // create root item in doc
  QDomElement doc_root = doc.createElement("plist");

  QDomAttr version = doc.createAttribute("version");
  version.setValue("1.0");

  doc_root.setAttributeNode(version);

  DomItem* item = m->itemForIndex(m->index(0, 0));
  strRootType = item->getType();
  QDomElement doc_dict = doc.createElement(strRootType);

  doc_root.appendChild(doc_dict);

  // add root item to doc
  doc.appendChild(doc_root);

  // get root item from model
  DomItem* model_root = m->getRoot()->child(0);

  // parse model recursively
  parseItem(model_root, doc_dict, doc);

  return doc;
}

void DomParser::parseElement(QDomElement& n, DomItem* item) {
  if (n.hasChildNodes()) {
    QDomElement element = n.firstChildElement();

    int nC = 1;

    while (!element.isNull()) {
      QString tagName = element.tagName();

      if (tagName == "key" || tagName == "array" || tagName == "dict" ||
          tagName == "string" || tagName == "integer" || tagName == "real" ||
          tagName == "data" || tagName == "date" || tagName == "bool") {
        // create child
        DomItem* domItem = item->addChild();

        if (tagName == "key") {
          // get key value
          QString eName = element.firstChild().nodeValue();

          // get val element
          QDomElement val = element.nextSiblingElement();

          // get type and val
          QString eType = val.tagName();

          QString eValue = val.firstChild().nodeValue();

          if (eType == "false") {
            eType = "bool";
            eValue = "false";
          }

          if (eType == "true") {
            eType = "bool";
            eValue = "true";
          }

          if (eType == "data") {
            QByteArray bytes = QByteArray::fromBase64(eValue.toUtf8());
            eValue = ByteToHexStr(bytes);
          }

          // set data
          domItem->setData(eName, eType, eValue);

          parseElement(val, domItem);

          element = val.nextSiblingElement();
        } else {
          domItem->setType(tagName);

          //新增：数组类型key为空的情况
          QString eName = element.firstChild().nodeValue();
          QString eType = element.tagName();

          QString eValue = element.firstChild().nodeValue();
          if (eType == "data") {
            QByteArray bytes = QByteArray::fromBase64(eValue.toUtf8());
            eValue = ByteToHexStr(bytes);
          }

          domItem->setData(eName, eType, eValue);

          domItem->setName(QString("Item %1").arg(nC++));
          // domItem->setName(QString::number(nC ++));

          parseElement(element, domItem);
          element = element.nextSiblingElement();
        }
      } else {
        qWarning("No value matching the key");

        break;
      }
    }
  }
}

void DomParser::parseItem(DomItem* item, QDomElement& n, QDomDocument& doc) {
  QDomElement element;
  QString name = item->getName();
  QString type = item->getType();
  QString value = item->getValue();

  if (name != "plist") {
    QRegExp rp("Item\\s\\d+");  //正则表达式，排除Item

    // item without key
    if (rp.exactMatch(name)) {
      // QDomElement container = doc.createElement(type);

      if (type == "bool") value = value.trimmed();

      if (type == "data") {
        QString v = value.remove(QRegExp("\\s"));  // 16进制去除所有空格
        value = QString::fromLatin1(HexStrToByte(v).toBase64());
      }

      // n.appendChild(container);
      // element = container;

    }  // else {

    QDomElement key;
    QDomText keyText;

    if (name.trimmed().mid(0, 4) == "Item") {
      keyText = doc.createTextNode(value);  //新增：解决数组无键值的情况
    } else {
      key = doc.createElement("key");
      keyText = doc.createTextNode(name);
    }

    if (!key.isNull()) key.appendChild(keyText);
    if (!n.isNull()) n.appendChild(key);

    QDomElement val;
    if (type == "bool")  //新增：bool类型
    {
      if (value.trimmed() == "true")
        val = doc.createElement(QStringLiteral("true"));
      if (value.trimmed() == "false")
        val = doc.createElement(QStringLiteral("false"));
    } else {
      val = doc.createElement(type);
    }

    n.appendChild(val);

    if (type != "array" && type != "dict" && type != "bool") {
      QDomText valText;

      if (type == "data")  //新增：解决16进制字串转换问题
      {
        value = value.remove(QRegExp("\\s"));  // 16进制去除所有空格

        if (name.trimmed().mid(0, 4) == "Item")
          valText = doc.createTextNode(value);
        else
          valText = doc.createTextNode(
              QString::fromLatin1(HexStrToByte(value).toBase64()));

      } else {
        valText = doc.createTextNode(value);
      }

      // qDebug() << valText.nodeValue();

      val.appendChild(valText);
    }

    element = val;
    //}

  } else
    element = n;

  // get children count
  int c = item->childCount();

  // iterate through children
  for (int i = 0; i < c; ++i) {
    DomItem* child = item->child(i);
    parseItem(child, element, doc);
  }
}

QString DomParser::ByteToHexStr(QByteArray ba) {
  QString str = ba.toHex().toUpper();

  return str;
}

QByteArray DomParser::HexStrToByte(QString value) {
  QByteArray ba;
  QVector<QString> byte;
  int len = value.length();
  int k = 0;
  ba.resize(len / 2);
  for (int i = 0; i < len / 2; i++) {
    // qDebug() << i << k;

    byte.push_back(value.mid(k, 2));
    ba[k / 2] = byte[k / 2].toUInt(nullptr, 16);
    k = k + 2;
  }

  return ba;
}
