## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

add_executable(lnw-example-tcp-echo lnw-example-tcp-echo.cpp)
target_link_libraries(lnw-example-tcp-echo PRIVATE lambdatech-networking)

add_executable(lnw-example-dns-dig lnw-example-dns-dig.cpp)
target_link_libraries(lnw-example-dns-dig PRIVATE lambdatech-networking)
