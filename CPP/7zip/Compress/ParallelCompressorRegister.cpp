// ParallelCompressorRegister.cpp - Registration for parallel compressor

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "ParallelCompressor.h"

namespace NCompress {
namespace NParallel {

REGISTER_CODEC_CREATE_2(CreateCodec, CParallelCompressor(), IParallelCompressor)

#ifndef Z7_EXTRACT_ONLY
REGISTER_CODEC_2(ParallelLZMA,
    CreateCodec,
    CreateCodec,
    0x999901, "ParallelLZMA")
#else
REGISTER_CODEC_VAR_2(ParallelLZMA, CreateCodec, NULL, 0x999901, "ParallelLZMA")
#endif

}}
