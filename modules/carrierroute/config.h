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
 * \brief Carrierroute :: Configuration
 * \ingroup carrierroute
 */


#ifndef _CARRIERROUTE_CONFIG_H
#define _CARRIERROUTE_CONFIG_H


#include "../../cfg/cfg.h"
#include "../../str.h"

struct cfg_group_carrierroute {
	unsigned int	use_domain;
	unsigned int	fallback_default;
	unsigned int	fetch_rows;
};

extern struct cfg_group_carrierroute	default_carrierroute_cfg;
extern void	*carrierroute_cfg;
extern cfg_def_t	carrierroute_cfg_def[];


#endif
