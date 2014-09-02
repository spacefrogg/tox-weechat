#ifndef TOX_WEECHAT_TOX_H
#define TOX_WEECHAT_TOX_H

#include <tox/tox.h>

/**
 * Initialize the Tox object, bootstrap the DHT and start working.
 */
void tox_weechat_tox_init();

/**
 * Dump Tox to file and de-initialize.
 */
void tox_weechat_tox_free();

#endif // TOX_WEECHAT_TOX_H