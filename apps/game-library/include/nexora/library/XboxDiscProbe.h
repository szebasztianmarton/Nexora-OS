#pragma once

#include <QString>

namespace nexora::library {

// True when the file holds an Xbox XDVDFS game partition: "MICROSOFT*XBOX*MEDIA" at
// 0x10000 past one of the known partition starts (rebuilt xiso, XGD3, XGD2). Reads at most
// three 20-byte blocks, never past the end of the file; any error means "not an Xbox disc".
// Original Xbox (XGD1) discs carry the same signature and are reported as well.
bool isXboxDisc(const QString &path);

} // namespace nexora::library
