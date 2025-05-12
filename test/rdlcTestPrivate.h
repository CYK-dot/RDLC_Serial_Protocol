#ifndef RDLCTESTPRIVATE_H_INCLUDED
#define RDLCTESTPRIVATE_H_INCLUDED


#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "rdlc.h"


/**
 *@brief 回调函数和接口模拟
**/
class RdlcMockCallback_t
{
    public:
        MOCK_METHOD(int, OnParsed, (Rdlc_t,RdlcAddr_t,const uint8_t*,uint16_t));
};

MATCHER_P2(EqWithMessage, expected, len, "") {
    return std::memcmp(arg, expected, len) == 0;
}
MATCHER_P2(AddrEq, expectedSrc, expectedDst, "") {
    return arg.srcAddr == expectedSrc && arg.dstAddr == expectedDst;
}


#endif // RDLCTESTPRIVATE_H_INCLUDED
