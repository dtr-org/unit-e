#include <esperanza/finalizationstate_data.h>

namespace esperanza {

FinalizationStateData::FinalizationStateData(const AdminParams &adminParams)
    : m_admin_state(adminParams) {}

bool FinalizationStateData::operator==(const FinalizationStateData &other) const {
  return m_checkpoints == other.m_checkpoints &&
         m_epoch_to_dynasty == other.m_epoch_to_dynasty &&
         m_dynasty_start_epoch == other.m_dynasty_start_epoch &&
         m_validators == other.m_validators &&
         m_dynasty_deltas == other.m_dynasty_deltas &&
         m_deposit_scale_factor == other.m_deposit_scale_factor &&
         m_total_slashed == other.m_total_slashed &&
         m_current_epoch == other.m_current_epoch &&
         m_current_dynasty == other.m_current_dynasty &&
         m_cur_dyn_deposits == other.m_cur_dyn_deposits &&
         m_prev_dyn_deposits == other.m_prev_dyn_deposits &&
         m_expected_source_epoch == other.m_expected_source_epoch &&
         m_last_finalized_epoch == other.m_last_finalized_epoch &&
         m_last_justified_epoch == other.m_last_justified_epoch &&
         m_recommended_target_hash == other.m_recommended_target_hash &&
         m_recommended_target_epoch == other.m_recommended_target_epoch &&
         m_last_voter_rescale == other.m_last_voter_rescale &&
         m_last_non_voter_rescale == other.m_last_non_voter_rescale &&
         m_reward_factor == other.m_reward_factor &&
         m_admin_state == other.m_admin_state;
}

}  // namespace esperanza
