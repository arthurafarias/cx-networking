// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Compile-time backend selection for the core::* facility façades
// (descriptor, poller, socket_ops, resolver) - see SRS-008 §2.2.
//
// Exactly one of LNW_NET_BACKEND_POSIX / LNW_NET_BACKEND_STANDALONE is 1.
// Override on the command line with -DLNW_NET_BACKEND_STANDALONE=1 (or =POSIX);
// otherwise POSIX is chosen on any Unix and STANDALONE everywhere else.

#if defined(LNW_NET_BACKEND_STANDALONE) && (LNW_NET_BACKEND_STANDALONE + 0)
#  undef LNW_NET_BACKEND_STANDALONE
#  define LNW_NET_BACKEND_STANDALONE 1
#  define LNW_NET_BACKEND_POSIX 0
#elif defined(LNW_NET_BACKEND_POSIX) && (LNW_NET_BACKEND_POSIX + 0)
#  undef LNW_NET_BACKEND_POSIX
#  define LNW_NET_BACKEND_POSIX 1
#  define LNW_NET_BACKEND_STANDALONE 0
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#  define LNW_NET_BACKEND_POSIX 1
#  define LNW_NET_BACKEND_STANDALONE 0
#else
#  define LNW_NET_BACKEND_POSIX 0
#  define LNW_NET_BACKEND_STANDALONE 1
#endif
