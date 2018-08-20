// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_QT_UNITEADDRESSVALIDATOR_H
#define UNITE_QT_UNITEADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class UnitEAddressEntryValidator : public QValidator {
  Q_OBJECT

 public:
  explicit UnitEAddressEntryValidator(QObject *parent);

  State validate(QString &input, int &pos) const;
};

/** UnitE address widget validator, checks for a valid unite address.
 */
class UnitEAddressCheckValidator : public QValidator {
  Q_OBJECT

 public:
  explicit UnitEAddressCheckValidator(QObject *parent);

  State validate(QString &input, int &pos) const;
};

#endif  // UNITE_QT_UNITEADDRESSVALIDATOR_H
