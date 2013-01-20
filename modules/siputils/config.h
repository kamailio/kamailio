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
 * \brief Siputils :: Configuration
 * \ingroup siputils
 */


#ifndef _SIPUTILS_CONFIG_H
#define _SIPUTILS_CONFIG_H


#include "../../cfg/cfg.h"
#include "../../str.h"

struct cfg_group_siputils {
	unsigned int	ring_timeout;
};

extern struct cfg_group_siputils	default_siputils_cfg;
extern void	*siputils_cfg;
extern cfg_def_t	siputils_cfg_def[];

int ring_timeout_fixup(void*, str*, str*, void**);

#endif
