## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

# Optional, external-dependency components live here, one directory per
# component, each its own dependency-detected CMake target - see the Plugin
# Folder Convention guideline under docs.src. The core library and every
# protocol under include/lambdatech/networking/ stay header-only and
# dependency-free beyond the STL and pthreads; anything that needs a
# third-party library (io_uring, OpenSSL for DoT, c-ares interop, ...) is
# opted into by name here (io_uring, OpenSSL for DoT, ngtcp2 + a TLS stack for
# the QUIC transport of SRS-004, c-ares interop, ...).
#
# No plugins exist yet. Add one with:
#   add_subdirectory(<name>)
