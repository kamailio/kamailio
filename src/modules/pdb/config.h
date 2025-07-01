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
 * \brief PDB :: Configuration
 * \ingroup rtpengine
 */


#ifndef _PDB_CONFIG_H
#define _PDB_CONFIG_H


#include "../../core/cfg/cfg.h"

struct cfg_group_pdb
{
	int timeout;
};

extern struct cfg_group_pdb default_pdb_cfg;
extern void *pdb_cfg;
extern cfg_def_t pdb_cfg_def[];


#endif
