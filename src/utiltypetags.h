// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TYPETAGS_H
#define UNIT_E_TYPETAGS_H

/*! \brief zero cost tagged types like Haskell's newtype
 *
 * See http://www.ilikebigbits.com/blog/2014/5/6/type-safe-identifiers-in-c
 *
 * @tparam Tag A phantom type to distinguish types
 * @tparam impl The implementing type
 * @tparam default_value A default value for the implementing type
 */
template <class Tag, class impl, impl default_value>
class Newtype
{
public:
    static Newtype invalid() { return Newtype(); }
    Newtype() : m_value(default_value) {}
    explicit Newtype(impl value) : m_value(value) {}
    explicit operator impl() const { return m_value; }
    friend bool operator==(Newtype a, Newtype b) { return a.m_value == b.m_value; }
    friend bool operator!=(Newtype a, Newtype b) { return a.m_value != b.m_value; }

private:
    impl m_value;
};

#endif //UNIT_E_TYPETAGS_H
