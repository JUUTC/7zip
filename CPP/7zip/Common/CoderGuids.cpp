// CoderGuids.cpp - GUID definitions for codec interfaces
// This file initializes the GUIDs for codec interfaces needed by CreateCoder/FilterCoder

#include "StdAfx.h"

// Must define INITGUID before including headers to initialize GUIDs
#include "../../Common/MyInitGuid.h"

// Include the interface declarations which contain Z7_DEFINE_GUID macros
#include "../ICoder.h"

// The GUIDs are automatically defined by the Z7_IFACE_CONSTR_CODER macros in ICoder.h:
// - IID_ICompressSetCoderProperties (0x20, group 4)
// - IID_ICompressWriteCoderProperties (0x23, group 4)
// - IID_ICompressSetCoderPropertiesOpt (0x1F, group 4)
// - IID_ICryptoResetInitVector (0x8C, group 4)
// - IID_ICompressSetDecoderProperties2 (0x22, group 4)
//
// The Z7_IFACE_CONSTR_CODER macro expands to Z7_DECL_IFACE_7ZIP which calls Z7_DEFINE_GUID.
// When INITGUID is defined (via MyInitGuid.h), Z7_DEFINE_GUID actually initializes the GUID
// instead of just declaring it as extern.
//
// GUID format: {23170F69-40C1-278A-0000-000<groupId>00<subId>0000}
// Example: ICompressSetCoderProperties has groupId=0x04, subId=0x20
//          GUID = {23170F69-40C1-278A-0000-000004002000}
