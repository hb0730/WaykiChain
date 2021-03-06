// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "id.h"
#include "persistence/accountdb.h"
#include "main.h"
#include "vm/wasm/types/name.hpp"
#include "vm/wasm/exception/exceptions.hpp"

extern CCacheDBManager *pCdMan;

CRegID::CRegID(const string &strRegID) { SetRegID(strRegID); }

CRegID::CRegID(const vector<uint8_t> &vIn) {
    assert(vIn.size() == 6);
    vRegID = vIn;
    height = 0;
    index  = 0;
    CDataStream ds(vIn, SER_DISK, CLIENT_VERSION);
    ds >> height;
    ds >> index;
}

CRegID::CRegID(const uint32_t heightIn, const uint16_t indexIn) {
    height = heightIn;
    index  = indexIn;
    vRegID.clear();
    vRegID.insert(vRegID.end(), BEGIN(heightIn), END(heightIn));
    vRegID.insert(vRegID.end(), BEGIN(indexIn), END(indexIn));
}

bool IsDigitalString(const string str){

    if (str.length() > 10 || str.length() == 0) //int max is 4294967295 can not over 10
        return false;

    for (auto te : str) {
        if (!isdigit(te))
            return false;
    }
    return true ;
}

bool CRegID::IsSimpleRegIdStr(const string & str) {
    int len = str.length();
    if (len >= 3) {
        int pos = str.find('-');

        if (pos > len - 1) {
            return false;
        }
        string firstStr = str.substr(0, pos);
        string endStr = str.substr(pos + 1);

        return IsDigitalString(firstStr) && IsDigitalString(endStr) ;
    }
    return false;
}

bool CRegID::GetKeyId(const string &str, CKeyID &keyId) {
    CRegID regId(str);
    if (regId.IsEmpty())
        return false;

    keyId = regId.GetKeyId(*pCdMan->pAccountCache);
    return !keyId.IsEmpty();
}

bool CRegID::IsRegIdStr(const string & str) {
    return ( IsSimpleRegIdStr(str) || (str.length() == 12) );
}

void CRegID::SetRegID(string strRegID) {
    height = 0;
    index  = 0;
    vRegID.clear();

    if (IsSimpleRegIdStr(strRegID)) {
        auto pos = strRegID.find('-');
        height   = atoi(strRegID.substr(0, pos).c_str());
        index    = atoi(strRegID.substr(pos + 1).c_str());
        vRegID.insert(vRegID.end(), BEGIN(height), END(height));
        vRegID.insert(vRegID.end(), BEGIN(index), END(index));
    } else if (strRegID.length() == 12) {
        vRegID = ::ParseHex(strRegID);

        if (vRegID.size() > sizeof(height) + sizeof(index)) {
            memcpy(&height, &vRegID[0], sizeof(height));
            memcpy(&index, &vRegID[sizeof(height)], sizeof(index));
        } else {
            // failed to parse strRegID, do not bother to initialize height and index.
        }
    }
}

void CRegID::SetRegID(const vector<uint8_t> &vIn) {
    assert(vIn.size() == 6);
    vRegID = vIn;
    CDataStream ds(vIn, SER_DISK, CLIENT_VERSION);
    ds >> height;
    ds >> index;
}

const vector<uint8_t> &CRegID::GetRegIdRaw() const {
    assert(vRegID.size() == 6);
    return vRegID;
}

bool CRegID::Clear() {
    height = 0;
    index  = 0;
    vRegID.clear();

    return true;
}

string CRegID::ToString() const {
    if (IsEmpty())
        return string("");

    return strprintf("%d-%d", height, index);
}

CKeyID CRegID::GetKeyId(const CAccountDBCache &accountCache) const {
    CKeyID retKeyId;
    accountCache.GetKeyId(*this, retKeyId);

    return retKeyId;
}

bool CRegID::IsMature(uint32_t curHeight) const {
    return ((height == 0) && (index != 0)) || ((height != 0) && curHeight > height + REG_ID_MATURITY);
}

void CRegID::SetRegIDByCompact(const vector<uint8_t> &vIn) {
    if (vIn.size() > 0) {
        CDataStream ds(vIn, SER_DISK, CLIENT_VERSION);
        ds >> *this;
    } else {
        Clear();
    }
}

///////////////////////////////////////////////////////////////////////////////
//class CNickID

CNickID::CNickID() {}

CNickID::CNickID(uint64_t nickIdIn): value(nickIdIn) {}

CNickID::CNickID(string nickIdIn) {
    try {
        value = wasm::name(nickIdIn).value;
    //} catch(wasm_chain::exception &e){
    //     value = 0 ;
    //     CHAIN_EXCEPTION_APPEND_LOG(e, log_level::warn,"'%s'", nickIdIn)
    // 
    }catch (...){
        value = 0 ;
    }

}

bool CNickID::IsMature(const uint32_t currHeight) const {

    uint32_t regHeight = 0 ;
    if(pCdMan->pAccountCache->GetNickIdHeight(value, regHeight) ){
        return currHeight > regHeight + NICK_ID_MATURITY ;
    }
    return false ;
}


bool CNickID::IsEmpty() const { return value == 0; }

void CNickID::SetEmpty() { value = 0; }

void CNickID::Clear() { value = 0; }

string CNickID::ToString() const { return wasm::name(value).to_string(); }

///////////////////////////////////////////////////////////////////////////////
// class CUserID

const CUserID CUserID::NULL_ID = {};
const EnumTypeMap<CUserID::VarIndex, string> CUserID::ID_NAME_MAP = {
    {IDX_NULL_ID, "Null"},
    {IDX_REG_ID, "RegID"},
    {IDX_KEY_ID, "KeyID"},
    {IDX_PUB_KEY_ID, "PubKey"},
    {IDX_NICK_ID, "NickID"}
};

string CUserID::ToDebugString() const {
    return std::visit([&](auto&& idIn) -> string {
        using ID = std::decay_t<decltype(idIn)>;
        if constexpr (std::is_same_v<ID, CRegID>) {
            return "R:" + idIn.ToString();
        } else if constexpr (std::is_same_v<ID, CKeyID>) {
            return "K:" + idIn.ToString() + ", addr=" + idIn.ToAddress();
        } else if constexpr (std::is_same_v<ID, CPubKey>) {
            return "P:" + idIn.ToString() + ", addr=" + idIn.GetKeyId().ToAddress();
        } else if constexpr (std::is_same_v<ID, CNickID>) {
            return "N:" + idIn.ToString();
        } else {  // CNullID
            assert( (std::is_same_v<ID, CNullID>) );
            return "Null";
        }
    }, uid);
}

shared_ptr<CUserID> CUserID::ParseUserId(const string &idStr) {
    CRegID regId(idStr);
    if (!regId.IsEmpty())
        return std::make_shared<CUserID>(regId);

    CKeyID keyId(idStr);
    if (!keyId.IsEmpty())
        return std::make_shared<CUserID>(keyId);

    // ParsePubKey
    auto pubKeyBin = ParseHex(idStr);
    CPubKey pubKey(pubKeyBin);
    if (pubKey.IsFullyValid())
        return std::make_shared<CUserID>(pubKey);

    CNickID nickId(idStr) ;

    if( pCdMan->pAccountCache->GetKeyId(nickId, keyId)){
        return std::make_shared<CUserID>(keyId);
    }


    return nullptr;
}

