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
 * \brief Outbound :: Configuration Framework support
 * \ingroup outbound
 */


#ifndef _OUTBOUND_CONFIG_H
#define _OUTBOUND_CONFIG_H

#include "../../qvalue.h"

#include "../../cfg/cfg.h"
#include "../../str.h"

struct cfg_group_outbound {
	int outbound_active;
};

extern struct cfg_group_outbound default_outbound_cfg;
extern void *outbound_cfg;
extern cfg_def_t outbound_cfg_def[];

#endif
