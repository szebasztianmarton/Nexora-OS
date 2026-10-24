#pragma once

#include <QString>

namespace nexora::library {

// True when the file holds an Xbox XDVDFS game partition: "MICROSOFT*XBOX*MEDIA" at
// 0x10000 past one of the known partition starts (rebuilt xiso, XGD3, XGD2). The file is
// opened non-blocking and only regular files are read (FIFOs, directories and devices are
// rejected). Reads at most three 20-byte blocks, never past the end of the file; any error
// means "not an Xbox disc". Rebuilt (xiso) original Xbox images also carry the signature and
// are reported as Xbox 360; full original Xbox disc dumps are not probed at their partition
// offset. Xenia does not run original Xbox games.
bool isXboxDisc(const QString &path);

} // namespace nexora::library
