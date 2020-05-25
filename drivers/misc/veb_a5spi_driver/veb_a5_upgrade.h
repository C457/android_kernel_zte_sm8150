#ifndef __VEB_A5_UPGRADE_H__
#define __VEB_A5_UPGRADE_H__

extern int veb_a5_upgrade(struct veb_private_data *priv, int type, char *cos, int cLength);
extern int veb_a5_restore_mode(struct veb_private_data *priv, int mode);

#if 0
extern int veb_a5_mode_switch(struct veb_private_data *priv, int mode);
#endif

#endif
