/*
 * $Id$
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file 
 * \brief P_USRLOC :: Configuration
 * \ingroup usrloc
 */


#ifndef _P_USRLOC_CONFIG_H
#define _P_USRLOC_CONFIG_H


#include "../../core/cfg/cfg.h"
#include "../../core/str.h"
#include "p_usrloc_mod.h"

struct cfg_group_p_usrloc
{
	unsigned int expire_time;
	unsigned int db_err_threshold;
	unsigned int failover_level;
	unsigned int db_ops_ruid;
	unsigned int db_update_as_insert;
	unsigned int matching_mode;
	unsigned int utc_timestamps;
};

extern struct cfg_group_p_usrloc default_p_usrloc_cfg;
extern void *p_usrloc_cfg;
extern cfg_def_t p_usrloc_cfg_def[];

extern void default_expires_stats_update(str *, str *);
extern void default_expires_range_update(str *, str *);
extern void max_expires_stats_update(str *, str *);

#endif
