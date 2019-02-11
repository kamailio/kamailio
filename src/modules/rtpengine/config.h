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
 * \brief Registrar :: Configuration
 * \ingroup rtpengine
 */


#ifndef _RTPENGINE_CONFIG_H
#define _RTPENGINE_CONFIG_H


#include "../../core/cfg/cfg.h"

#define MAX_RTPP_TRIED_NODES            30

struct cfg_group_rtpengine {
	unsigned int	rtpengine_disable_tout;
	unsigned int	aggressive_redetection;
	unsigned int	rtpengine_tout_ms;
	unsigned int    queried_nodes_limit;
	unsigned int	rtpengine_retr;
};

extern struct cfg_group_rtpengine	default_rtpengine_cfg;
extern void	*rtpengine_cfg;
extern cfg_def_t	rtpengine_cfg_def[];


#endif
