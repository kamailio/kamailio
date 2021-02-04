/*
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Registrar :: Configuration
 * \ingroup registrar
 */


#ifndef _REGISTRAR_CONFIG_H
#define _REGISTRAR_CONFIG_H

#include "../../core/qvalue.h"

#include "../../core/cfg/cfg.h"
#include "../../core/str.h"

struct cfg_group_registrar {
	str 		realm_pref;
	unsigned int	default_expires;
	unsigned int	default_expires_range;
	unsigned int	expires_range;
	unsigned int	min_expires;
	unsigned int	max_expires;
	unsigned int	max_contacts;
	unsigned int	retry_after;
	unsigned int	case_sensitive;
	qvalue_t	default_q;
	unsigned int	append_branches;
	unsigned int	use_expired_contacts;
};

extern struct cfg_group_registrar	default_registrar_cfg;
extern void	*registrar_cfg;
extern cfg_def_t	registrar_cfg_def[];

extern void default_expires_stats_update(str*, str*);
extern void default_expires_range_update(str*, str*);
extern void expires_range_update(str*, str*);
extern void max_expires_stats_update(str*, str*);

#endif
