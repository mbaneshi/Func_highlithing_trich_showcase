;; Elector smartcontract

;; cur_elect credits past_elections grams active_id active_hash
(cell, cell, cell, int, int, int) load_data() inline_ref {
  var cs = get_data().begin_parse();
  var res = (cs~load_dict(), cs~load_dict(), cs~load_dict(), cs~load_grams(), cs~load_uint(32), cs~load_uint(256));
  cs.end_parse();
  return res;
}

;; cur_elect credits past_elections grams active_id active_hash
() store_data(elect, credits, past_elections, grams, active_id, active_hash) impure inline_ref {
  set_data(begin_cell()
    .store_dict(elect)
    .store_dict(credits)
    .store_dict(past_elections)
    .store_grams(grams)
    .store_uint(active_id, 32)
    .store_uint(active_hash, 256)
  .end_cell());
}

;; elect -> elect_at elect_close min_stake total_stake members failed finished
_ unpack_elect(elect) inline_ref {
  var es = elect.begin_parse();
  var res = (es~load_uint(32), es~load_uint(32), es~load_grams(), es~load_grams(), es~load_dict(), es~load_int(1), es~load_int(1));
  es.end_parse();
  return res;
}

cell pack_elect(elect_at, elect_close, min_stake, total_stake, members, failed, finished) inline_ref {
  return begin_cell()
    .store_uint(elect_at, 32)
    .store_uint(elect_close, 32)
    .store_grams(min_stake)
    .store_grams(total_stake)
    .store_dict(members)
    .store_int(failed, 1)
    .store_int(finished, 1)
  .end_cell();
}

;; slice -> unfreeze_at stake_held vset_hash frozen_dict total_stake bonuses complaints
_ unpack_past_election(slice fs) inline_ref {
  var res = (fs~load_uint(32), fs~load_uint(32), fs~load_uint(256), fs~load_dict(), fs~load_grams(), fs~load_grams(), fs~load_dict());
  fs.end_parse();
  return res;
}

builder pack_past_election(int unfreeze_at, int stake_held, int vset_hash, cell frozen_dict, int total_stake, int bonuses, cell complaints) inline_ref {
  return begin_cell()
      .store_uint(unfreeze_at, 32)
      .store_uint(stake_held, 32)
      .store_uint(vset_hash, 256)
      .store_dict(frozen_dict)
      .store_grams(total_stake)
      .store_grams(bonuses)
      .store_dict(complaints);
}

;; complaint_status#2d complaint:^ValidatorComplaint voters:(HashmapE 16 True)
;;   vset_id:uint256 weight_remaining:int64 = ValidatorComplaintStatus;
_ unpack_complaint_status(slice cs) inline_ref {
  throw_unless(9, cs~load_uint(8) == 0x2d);
  var res = (cs~load_ref(), cs~load_dict(), cs~load_uint(256), cs~load_int(64));
  cs.end_parse();
  return res;
}

builder pack_complaint_status(cell complaint, cell voters, int vset_id, int weight_remaining) inline_ref {
  return begin_cell()
    .store_uint(0x2d, 8)
    .store_ref(complaint)
    .store_dict(voters)
    .store_uint(vset_id, 256)
    .store_int(weight_remaining, 64);
}

;; validator_complaint#bc validator_pubkey:uint256 description:^ComplaintDescr
;;   created_at:uint32 severity:uint8 reward_addr:uint256 paid:Grams suggested_fine:Grams
;;   suggested_fine_part:uint32 = ValidatorComplaint;
_ unpack_complaint(slice cs) inline_ref {
  throw_unless(9, cs~load_int(8) == 0xbc - 0x100);
  var res = (cs~load_uint(256), cs~load_ref(), cs~load_uint(32), cs~load_uint(8), cs~load_uint(256), cs~load_grams(), cs~load_grams(), cs~load_uint(32));
  cs.end_parse();
  return res;
}

builder pack_complaint(int validator_pubkey, cell description, int created_at, int severity, int reward_addr, int paid, int suggested_fine, int suggested_fine_part) inline_ref {
  return begin_cell()
    .store_int(0xbc - 0x100, 8)
    .store_uint(validator_pubkey, 256)
    .store_ref(description)
    .store_uint(created_at, 32)
    .store_uint(severity, 8)
    .store_uint(reward_addr, 256)
    .store_grams(paid)
    .store_grams(suggested_fine)
    .store_uint(suggested_fine_part, 32);
}

;; complaint_prices#1a deposit:Grams bit_price:Grams cell_price:Grams = ComplaintPricing; 
(int, int, int) parse_complaint_prices(cell info) inline {
  var cs = info.begin_parse();
  throw_unless(9, cs~load_uint(8) == 0x1a);
  var res = (cs~load_grams(), cs~load_grams(), cs~load_grams());
  cs.end_parse();
  return res;
}

;; deposit bit_price cell_price
(int, int, int) get_complaint_prices() inline_ref {
  var info = config_param(13);
  return info.null?() ? (1 << 36, 1, 512) : info.parse_complaint_prices();
}

;; elected_for elections_begin_before elections_end_before stake_held_for
(int, int, int, int) get_validator_conf() {
  var cs = config_param(15).begin_parse();
  return (cs~load_int(32), cs~load_int(32), cs~load_int(32), cs.preload_int(32));
}

;; next three functions return information about current validator set (config param #34)
;; they are borrowed from config-code.fc
(cell, int, cell) get_current_vset() inline_ref {
  var vset = config_param(34);
  var cs = begin_parse(vset);
  ;; validators_ext#12 utime_since:uint32 utime_until:uint32 
  ;; total:(## 16) main:(## 16) { main <= total } { main >= 1 } 
  ;; total_weight:uint64
  throw_unless(40, cs~load_uint(8) == 0x12);
  cs~skip_bits(32 + 32 + 16 + 16);
  var (total_weight, dict) = (cs~load_uint(64), cs~load_dict());
  cs.end_parse();
  return (vset, total_weight, dict);
}

(slice, int) get_validator_descr(int idx) inline_ref {
  var (vset, total_weight, dict) = get_current_vset();
  var (value, _) = dict.udict_get?(16, idx);
  return (value, total_weight);
}

(int, int) unpack_validator_descr(slice cs) inline {
  ;; ed25519_pubkey#8e81278a pubkey:bits256 = SigPubKey;
  ;; validator#53 public_key:SigPubKey weight:uint64 = ValidatorDescr;
  ;; validator_addr#73 public_key:SigPubKey weight:uint64 adnl_addr:bits256 = ValidatorDescr;
  throw_unless(41, (cs~load_uint(8) & ~ 0x20) == 0x53);
  throw_unless(41, cs~load_uint(32) == 0x8e81278a);
  return (cs~load_uint(256), cs~load_uint(64));
}

() send_message_back(addr, ans_tag, query_id, body, grams, mode) impure inline_ref {
  ;; int_msg_info$0 ihr_disabled:Bool bounce:Bool bounced:Bool src:MsgAddress -> 011000
  var msg = begin_cell()
    .store_uint(0x18, 6)
    .store_slice(addr)
    .store_grams(grams)
    .store_uint(0, 1 + 4 + 4 + 64 + 32 + 1 + 1)
    .store_uint(ans_tag, 32)
    .store_uint(query_id, 64);
  if (body >= 0) {
    msg~store_uint(body, 32);
  }
  send_raw_message(msg.end_cell(), mode);
}

() return_stake(addr, query_id, reason) impure inline_ref {
  return send_message_back(addr, 0xee6f454c, query_id, reason, 0, 64);
}

() send_confirmation(addr, query_id, comment) impure inline_ref {
  return send_message_back(addr, 0xf374484c, query_id, comment, 1000000000, 2);
}

() send_validator_set_to_config(config_addr, vset, query_id) impure inline_ref {
  var msg = begin_cell()
    .store_uint(0xc4ff, 17)   ;; 0 11000100 0xff 
    .store_uint(config_addr, 256)
    .store_grams(1 << 30)     ;; ~1 gram of value to process and obtain answer
    .store_uint(0, 1 + 4 + 4 + 64 + 32 + 1 + 1)
    .store_uint(0x4e565354, 32)
    .store_uint(query_id, 64)
    .store_ref(vset);
  send_raw_message(msg.end_cell(), 1);
}

;; credits 'amount' to 'addr' inside credit dictionary 'credits'
_ ~credit_to(credits, addr, amount) inline_ref {
  var (val, f) = credits.udict_get?(256, addr);
  if (f) {
    amount += val~load_grams();
  }
  credits~udict_set_builder(256, addr, begin_cell().store_grams(amount));
  return (credits, ());
}

() process_new_stake(s_addr, msg_value, cs, query_id) impure inline_ref {
  var (src_wc, src_addr) = parse_std_addr(s_addr);
  var ds = get_data().begin_parse();
  var elect = ds~load_dict();
  if (elect.null?() | (src_wc + 1)) {
    ;; no elections active, or source is not in masterchain
    ;; bounce message
    return return_stake(s_addr, query_id, 0);
  }
  ;; parse the remainder of new stake message
  var validator_pubkey = cs~load_uint(256);
  var stake_at = cs~load_uint(32);
  var max_factor = cs~load_uint(32);
  var adnl_addr = cs~load_uint(256);
  var signature = cs~load_ref().begin_parse().preload_bits(512);
  cs.end_parse();
  ifnot (check_data_signature(begin_cell()
      .store_uint(0x654c5074, 32)
      .store_uint(stake_at, 32)
      .store_uint(max_factor, 32)
      .store_uint(src_addr, 256)
      .store_uint(adnl_addr, 256)
    .end_cell().begin_parse(), signature, validator_pubkey)) {
    ;; incorrect signature, return stake
    return return_stake(s_addr, query_id, 1);
  }
  if (max_factor < 0x10000) {
    ;; factor must be >= 1. = 65536/65536
    return return_stake(s_addr, query_id, 6);
  }
  ;; parse current election data
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  ;; elect_at~dump();
  msg_value -= 1000000000;   ;; deduct GR$1 for sending confirmation
  if ((msg_value << 12) < total_stake) {
    ;; stake smaller than 1/4096 of the total accumulated stakes, return
    return return_stake(s_addr, query_id, 2);
  }
  total_stake += msg_value;  ;; (provisionally) increase total stake
  if (stake_at != elect_at) {
    ;; stake for some other elections, return
    return return_stake(s_addr, query_id, 3);
  }
  if (finished) {
    ;; elections already finished, return stake
    return return_stake(s_addr, query_id, 0);
  }
  var (mem, found) = members.udict_get?(256, validator_pubkey);
  if (found) {
    ;; entry found, merge stakes
    msg_value += mem~load_grams();
    mem~load_uint(64);   ;; skip timestamp and max_factor
    found = (src_addr != mem~load_uint(256));
  }
  if (found) {
    ;; can make stakes for a public key from one address only
    return return_stake(s_addr, query_id, 4);
  }
  if (msg_value < min_stake) {
    ;; stake too small, return it
    return return_stake(s_addr, query_id, 5);
  }
  throw_unless(44, msg_value);
  accept_message();
  ;; store stake in the dictionary
  members~udict_set_builder(256, validator_pubkey, begin_cell()
    .store_grams(msg_value)
    .store_uint(now(), 32)
    .store_uint(max_factor, 32)
    .store_uint(src_addr, 256)
    .store_uint(adnl_addr, 256));
  ;; gather and save election data
  elect = pack_elect(elect_at, elect_close, min_stake, total_stake, members, false, false);
  set_data(begin_cell().store_dict(elect).store_slice(ds).end_cell());
  ;; return confirmation message
  if (query_id) {
    return send_confirmation(s_addr, query_id, 0);
  }
  return ();
}

(cell, int) unfreeze_without_bonuses(credits, freeze_dict, tot_stakes) inline_ref {
  var total = var recovered = 0;
  var pubkey = -1;
  do {
    (pubkey, var cs, var f) = freeze_dict.udict_get_next?(256, pubkey);
    if (f) {
      var (addr, weight, stake, banned) = (cs~load_uint(256), cs~load_uint(64), cs~load_grams(), cs~load_int(1));
      cs.end_parse();
      if (banned) {
        recovered += stake;
      } else {
        credits~credit_to(addr, stake);
      }
      total += stake;
    }
  } until (~ f);
  throw_unless(59, total == tot_stakes);
  return (credits, recovered);
}

(cell, int) unfreeze_with_bonuses(credits, freeze_dict, tot_stakes, tot_bonuses) inline_ref {
  var total = var recovered = var returned_bonuses = 0;
  var pubkey = -1;
  do {
    (pubkey, var cs, var f) = freeze_dict.udict_get_next?(256, pubkey);
    if (f) {
      var (addr, weight, stake, banned) = (cs~load_uint(256), cs~load_uint(64), cs~load_grams(), cs~load_int(1));
      cs.end_parse();
      if (banned) {
        recovered += stake;
      } else {
        var bonus = muldiv(tot_bonuses, stake, tot_stakes);
        returned_bonuses += bonus;
        credits~credit_to(addr, stake + bonus);
      }
      total += stake;
    }
  } until (~ f);
  throw_unless(59, (total == tot_stakes) & (returned_bonuses <= tot_bonuses));
  return (credits, recovered + tot_bonuses - returned_bonuses);
}

int stakes_sum(frozen_dict) inline_ref {
  var total = 0;
  var pubkey = -1;
  do {
    (pubkey, var cs, var f) = frozen_dict.udict_get_next?(256, pubkey);
    if (f) {
      cs~skip_bits(256 + 64);
      total += cs~load_grams();
    }
  } until (~ f);
  return total;
}

_ unfreeze_all(credits, past_elections, elect_id) inline_ref {
  var (fs, f) = past_elections~udict_delete_get?(32, elect_id);
  ifnot (f) {
    ;; no elections with this id
    return (credits, past_elections, 0);
  }
  var (unfreeze_at, stake_held, vset_hash, fdict, tot_stakes, bonuses, complaints) = fs.unpack_past_election();
  ;; tot_stakes = fdict.stakes_sum(); ;; TEMP BUGFIX
  var unused_prizes = (bonuses > 0) ?
    credits~unfreeze_with_bonuses(fdict, tot_stakes, bonuses) :
    credits~unfreeze_without_bonuses(fdict, tot_stakes);
  return (credits, past_elections, unused_prizes);
}

() config_set_confirmed(s_addr, cs, query_id, ok) impure inline_ref {
  var (src_wc, src_addr) = parse_std_addr(s_addr);
  var config_addr = config_param(0).begin_parse().preload_uint(256);
  var ds = get_data().begin_parse();
  var elect = ds~load_dict();
  if ((src_wc + 1) | (src_addr != config_addr) | elect.null?()) {
    ;; not from config smc, somebody's joke?
    ;; or no elections active (or just completed)
    return ();
  }
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  if ((elect_at != query_id) | ~ finished) {
    ;; not these elections, or elections not finished yet
    return ();
  }
  accept_message();
  ifnot (ok) {
    ;; cancel elections, return stakes
    var (credits, past_elections, grams) = (ds~load_dict(), ds~load_dict(), ds~load_grams());
    (credits, past_elections, var unused_prizes) = unfreeze_all(credits, past_elections, elect_at);
    set_data(begin_cell()
      .store_int(false, 1)
      .store_dict(credits)
      .store_dict(past_elections)
      .store_grams(grams + unused_prizes)
      .store_slice(ds)
    .end_cell());
  }
  ;; ... do not remove elect until we see this set as the next elected validator set
}

() process_simple_transfer(s_addr, msg_value) impure inline_ref {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  (int src_wc, int src_addr) = parse_std_addr(s_addr);
  if (src_addr | (src_wc + 1) | (active_id == 0)) {
    ;; simple transfer to us (credit "nobody's" account)
    ;; (or no known active validator set)
    grams += msg_value;
    return store_data(elect, credits, past_elections, grams, active_id, active_hash);
  }
  ;; zero source address -1:00..00 (collecting validator fees)
  var (fs, f) = past_elections.udict_get?(32, active_id);
  ifnot (f) {
    ;; active validator set not found (?)
    grams += msg_value;
  } else {
    ;; credit active validator set bonuses
    var (unfreeze_at, stake_held, hash, dict, total_stake, bonuses, complaints) = fs.unpack_past_election();
    bonuses += msg_value;
    past_elections~udict_set_builder(32, active_id,
      pack_past_election(unfreeze_at, stake_held, hash, dict, total_stake, bonuses, complaints));
  }
  return store_data(elect, credits, past_elections, grams, active_id, active_hash);
}

() recover_stake(op, s_addr, cs, query_id) impure inline_ref {
  (int src_wc, int src_addr) = parse_std_addr(s_addr);
  if (src_wc + 1) {
    ;; not from masterchain, return error
    return send_message_back(s_addr, 0xfffffffe, query_id, op, 0, 64);
  }
  var ds = get_data().begin_parse();
  var (elect, credits) = (ds~load_dict(), ds~load_dict());
  var (cs, f) = credits~udict_delete_get?(256, src_addr);
  ifnot (f) {
    ;; no credit for sender, return error
    return send_message_back(s_addr, 0xfffffffe, query_id, op, 0, 64);
  }
  var amount = cs~load_grams();
  cs.end_parse();
  ;; save data
  set_data(begin_cell().store_dict(elect).store_dict(credits).store_slice(ds).end_cell());
  ;; send amount to sender in a new message
  send_raw_message(begin_cell()
    .store_uint(0x18, 6)
    .store_slice(s_addr)
    .store_grams(amount)
    .store_uint(0, 1 + 4 + 4 + 64 + 32 + 1 + 1)
    .store_uint(0xf96f7324, 32)
    .store_uint(query_id, 64)
  .end_cell(), 64);
}

() after_code_upgrade(slice s_addr, slice cs, int query_id) impure method_id(1666) {
  var op = 0x4e436f64;
  return send_message_back(s_addr, 0xce436f64, query_id, op, 0, 64);
}

int upgrade_code(s_addr, cs, query_id) inline_ref {
  var c_addr = config_param(0);
  if (c_addr.null?()) {
    ;; no configuration smart contract known
    return false;
  }
  var config_addr = c_addr.begin_parse().preload_uint(256);
  var (src_wc, src_addr) = parse_std_addr(s_addr);
  if ((src_wc + 1) | (src_addr != config_addr)) {
    ;; not from configuration smart contract, return error
    return false;
  }
  accept_message();
  var code = cs~load_ref();
  set_code(code);
  ifnot(cs.slice_empty?()) {
    set_c3(code.begin_parse().bless());
    after_code_upgrade(s_addr, cs, query_id);
    throw(0);
  }
  return true;
}

int register_complaint(s_addr, complaint, msg_value) {
  var (src_wc, src_addr) = parse_std_addr(s_addr);
  if (src_wc + 1) { ;; not from masterchain, return error
    return -1;
  }
  if (complaint.slice_depth() >= 128) {
    return -3;  ;; invalid complaint
  }
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var election_id = complaint~load_uint(32);
  var (fs, f) = past_elections.udict_get?(32, election_id);
  ifnot (f) {  ;; election not found
    return -2;
  }
  var expire_in = fs.preload_uint(32) - now();
  if (expire_in <= 0) { ;; already expired
    return -4;
  }
  var (validator_pubkey, description, created_at, severity, reward_addr, paid, suggested_fine, suggested_fine_part) = unpack_complaint(complaint);
  reward_addr = src_addr;
  created_at = now();
  ;; compute complaint storage/creation price
  var (deposit, bit_price, cell_price) = get_complaint_prices();
  var (_, bits, refs) = slice_compute_data_size(complaint, 4096);
  var pps = (bits + 1024) * bit_price + (refs + 2) * cell_price;
  paid = pps * expire_in + deposit;
  if (msg_value < paid + (1 << 30)) { ;; not enough money
    return -5;
  }
  ;; re-pack modified complaint
  cell complaint = pack_complaint(validator_pubkey, description, created_at, severity, reward_addr, paid, suggested_fine, suggested_fine_part).end_cell();
  var (unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints) = unpack_past_election(fs);
  var (fs, f) = frozen_dict.udict_get?(256, validator_pubkey);
  ifnot (f) { ;; no such validator, cannot complain
    return -6;
  }
  fs~skip_bits(256 + 64);   ;; addr weight
  var validator_stake = fs~load_grams();
  int fine = suggested_fine + muldiv(validator_stake, suggested_fine_part, 1 << 32);
  if (fine > validator_stake) {  ;; validator's stake is less than suggested fine
    return -7;
  }
  if (fine <= paid) {  ;; fine is less than the money paid for creating complaint
    return -8;
  }
  ;; create complaint status
  var cstatus = pack_complaint_status(complaint, null(), 0, 0);
  ;; save complaint status into complaints
  var cpl_id = complaint.cell_hash();
  ifnot (complaints~udict_add_builder?(256, cpl_id, cstatus)) {
    return -9;   ;; complaint already exists
  }
  ;; pack past election info
  past_elections~udict_set_builder(32, election_id, pack_past_election(unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints));
  ;; pack persistent data
  ;; next line can be commented, but it saves a lot of stack manipulations
  var (elect, credits, _, grams, active_id, active_hash) = load_data();
  store_data(elect, credits, past_elections, grams, active_id, active_hash);
  return paid;
}

(cell, cell, int, int) punish(credits, frozen, complaint) inline_ref {
  var (validator_pubkey, description, created_at, severity, reward_addr, paid, suggested_fine, suggested_fine_part) = complaint.begin_parse().unpack_complaint();
  var (cs, f) = frozen.udict_get?(256, validator_pubkey);
  ifnot (f) {
    ;; no validator to punish
    return (credits, frozen, 0, 0);
  }
  var (addr, weight, stake, banned) = (cs~load_uint(256), cs~load_uint(64), cs~load_grams(), cs~load_int(1));
  cs.end_parse();
  int fine = min(stake, suggested_fine + muldiv(stake, suggested_fine_part, 1 << 32));
  stake -= fine;
  frozen~udict_set_builder(256, validator_pubkey, begin_cell()
    .store_uint(addr, 256)
    .store_uint(weight, 64)
    .store_grams(stake)
    .store_int(banned, 1));
  int reward = min(fine >> 3, paid * 8);
  credits~credit_to(reward_addr, reward);
  return (credits, frozen, fine - reward, fine);
}

(cell, cell, int) register_vote(complaints, chash, idx, weight) inline_ref {
  var (cstatus, found?) = complaints.udict_get?(256, chash);
  ifnot (found?) {
    ;; complaint not found
    return (complaints, null(), -1);
  }
  var (cur_vset, total_weight, _) = get_current_vset();
  int cur_vset_id = cur_vset.cell_hash();
  var (complaint, voters, vset_id, weight_remaining) = unpack_complaint_status(cstatus);
  int vset_old? = (vset_id != cur_vset_id);
  if ((weight_remaining < 0) & vset_old?) {
    ;; previous validator set already collected 2/3 votes, skip new votes
    return (complaints, null(), -3);
  }
  if (vset_old?) {
    ;; complaint votes belong to a previous validator set, reset voting
    vset_id = cur_vset_id;
    voters = null();
    weight_remaining = muldiv(total_weight, 2, 3);
  }
  var (_, found?) = voters.udict_get?(16, idx);
  if (found?) {
    ;; already voted for this proposal, ignore vote
    return (complaints, null(), 0);
  }
  ;; register vote
  voters~udict_set_builder(16, idx, begin_cell().store_uint(now(), 32));
  int old_wr = weight_remaining;
  weight_remaining -= weight;
  old_wr ^= weight_remaining;
  ;; save voters and weight_remaining
  complaints~udict_set_builder(256, chash, pack_complaint_status(complaint, voters, vset_id, weight_remaining));
  if (old_wr >= 0) {
    ;; not enough votes or already accepted
    return (complaints, null(), 1);
  }
  ;; complaint wins, prepare punishment
  return (complaints, complaint, 2);
}

int proceed_register_vote(election_id, chash, idx, weight) impure inline_ref {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var (fs, f) = past_elections.udict_get?(32, election_id);
  ifnot (f) {  ;; election not found
    return -2;
  }
  var (unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints) = unpack_past_election(fs);
  (complaints, var accepted_complaint, var status) = register_vote(complaints, chash, idx, weight);
  if (status <= 0) {
    return status;
  }
  ifnot (accepted_complaint.null?()) {
    (credits, frozen_dict, int fine_unalloc, int fine_collected) = punish(credits, frozen_dict, accepted_complaint);
    grams += fine_unalloc;
    total_stake -= fine_collected;
  }
  past_elections~udict_set_builder(32, election_id, pack_past_election(unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints));
  store_data(elect, credits, past_elections, grams, active_id, active_hash);
  return status;
}

() recv_internal(int msg_value, cell in_msg_cell, slice in_msg) impure {
  ;; do nothing for internal messages
  var cs = in_msg_cell.begin_parse();
  var flags = cs~load_uint(4);  ;; int_msg_info$0 ihr_disabled:Bool bounce:Bool bounced:Bool
  if (flags & 1) {
    ;; ignore all bounced messages
    return ();
  }
  var s_addr = cs~load_msg_addr();
  if (in_msg.slice_empty?()) {
    ;; inbound message has empty body
    return process_simple_transfer(s_addr, msg_value);
  }
  int op = in_msg~load_uint(32);
  if (op == 0) {
    ;; simple transfer with comment, return
    return process_simple_transfer(s_addr, msg_value);
  }
  int query_id = in_msg~load_uint(64);
  if (op == 0x4e73744b) {
    ;; new stake message
    return process_new_stake(s_addr, msg_value, in_msg, query_id);
  }
  if (op == 0x47657424) {
    ;; recover stake request
    return recover_stake(op, s_addr, in_msg, query_id);
  }
  if (op == 0x4e436f64) {
    ;; upgrade code (accepted only from configuration smart contract)
    var ok = upgrade_code(s_addr, in_msg, query_id);
    return send_message_back(s_addr, ok ? 0xce436f64 : 0xffffffff, query_id, op, 0, 64);
  }
  var cfg_ok = (op == 0xee764f4b);
  if (cfg_ok | (op == 0xee764f6f)) {
    ;; confirmation from configuration smart contract
    return config_set_confirmed(s_addr, in_msg, query_id, cfg_ok);
  }
  if (op == 0x52674370) {
    ;; new complaint 
    var price = register_complaint(s_addr, in_msg, msg_value);
    int mode = 64;
    int ans_tag = - price;
    if (price >= 0) {
      ;; ok, debit price
      raw_reserve(price, 4);
      ans_tag = 0;
      mode = 128;
    }
    return send_message_back(s_addr, ans_tag + 0xf2676350, query_id, op, 0, mode);
  }
  if (op == 0x56744370) {
    ;; vote for a complaint
    var signature = in_msg~load_bits(512);
    var msg_body = in_msg;
    var (sign_tag, idx, elect_id, chash) = (in_msg~load_uint(32), in_msg~load_uint(16), in_msg~load_uint(32), in_msg~load_uint(256));
    in_msg.end_parse();
    throw_unless(37, sign_tag == 0x56744350);
    var (vdescr, total_weight) = get_validator_descr(idx);
    var (val_pubkey, weight) = unpack_validator_descr(vdescr);
    throw_unless(34, check_data_signature(msg_body, signature, val_pubkey));
    int res = proceed_register_vote(elect_id, chash, idx, weight);
    return send_message_back(s_addr, res + 0xd6745240, query_id, op, 0, 64);
  }

  ifnot (op & (1 << 31)) {
    ;; unknown query, return error
    return send_message_back(s_addr, 0xffffffff, query_id, op, 0, 64);
  }
  ;; unknown answer, ignore
  return ();
}

int postpone_elections() impure {
  return false;
}

;; computes the total stake out of the first n entries of list l
_ compute_total_stake(l, n, m_stake) inline_ref {
  int tot_stake = 0;
  repeat (n) {
    (var h, l) = uncons(l);
    var stake = h.at(0);
    var max_f = h.at(1);
    stake = min(stake, (max_f * m_stake) >> 16);
    tot_stake += stake;
  }
  return tot_stake;
}

(cell, cell, int, cell, int, int) try_elect(credits, members, min_stake, max_stake, min_total_stake, max_stake_factor) {
  var cs = 16.config_param().begin_parse();
  var (max_validators, _, min_validators) = (cs~load_uint(16), cs~load_uint(16), cs~load_uint(16));
  cs.end_parse();
  min_validators = max(min_validators, 1);
  int n = 0;
  var sdict = new_dict();
  var pubkey = -1;
  do {
    (pubkey, var cs, var f) = members.udict_get_next?(256, pubkey);
    if (f) {
      var (stake, time, max_factor, addr, adnl_addr) = (cs~load_grams(), cs~load_uint(32), cs~load_uint(32), cs~load_uint(256), cs~load_uint(256));
      cs.end_parse();
      var key = begin_cell()
        .store_uint(stake, 128)
        .store_int(- time, 32)
        .store_uint(pubkey, 256)
      .end_cell().begin_parse();
      sdict~dict_set_builder(128 + 32 + 256, key, begin_cell()
          .store_uint(min(max_factor, max_stake_factor), 32)
          .store_uint(addr, 256)
          .store_uint(adnl_addr, 256));
      n += 1;
    }
  } until (~ f);
  n = min(n, max_validators);
  if (n < min_validators) {
    return (credits, new_dict(), 0, new_dict(), 0, 0);
  }
  var l = nil;
  do {
    var (key, cs, f) = sdict~dict::delete_get_min(128 + 32 + 256);
    if (f) {
      var (stake, _, pubkey) = (min(key~load_uint(128), max_stake), key~load_uint(32), key.preload_uint(256));
      var (max_f, _, adnl_addr) = (cs~load_uint(32), cs~load_uint(256), cs.preload_uint(256));
      l = cons([stake, max_f, pubkey, adnl_addr], l);
    }
  } until (~ f);
  ;; l is the list of all stakes in decreasing order
  int i = min_validators - 1;
  var l1 = l;
  repeat (i) {
    l1 = cdr(l1);
  }
  var (best_stake, m) = (0, 0);
  do {
    var stake = l1~list_next().at(0);
    i += 1;
    if (stake >= min_stake) {
      var tot_stake = compute_total_stake(l, i, stake);
      if (tot_stake > best_stake) {
        (best_stake, m) = (tot_stake, i);
      }
    }
  } until (i >= n);
  if ((m == 0) | (best_stake < min_total_stake)) {
    return (credits, new_dict(), 0, new_dict(), 0, 0);
  }
  ;; we have to select first m validators from list l
  l1 = touch(l);
  ;; l1~dump();  ;; DEBUG
  repeat (m - 1) {
    l1 = cdr(l1);
  }
  var m_stake = car(l1).at(0);  ;; minimal stake
  ;; create both the new validator set and the refund set
  int i = 0;
  var tot_stake = 0;
  var tot_weight = 0;
  var vset = new_dict();
  var frozen = new_dict();
  do {
    var [stake, max_f, pubkey, adnl_addr] = l~list_next();
    ;; lookup source address first
    var (val, f) = members.udict_get?(256, pubkey);
    throw_unless(61, f);
    (_, _, var src_addr) = (val~load_grams(), val~load_uint(64), val.preload_uint(256));
    if (i < m) {
      ;; one of the first m members, include into validator set
      var true_stake = min(stake, (max_f * m_stake) >> 16);
      stake -= true_stake;
      ;; ed25519_pubkey#8e81278a pubkey:bits256 = SigPubKey;  // 288 bits
      ;; validator_addr#73 public_key:SigPubKey weight:uint64 adnl_addr:bits256 = ValidatorDescr;
      var weight = (true_stake << 60) / best_stake;
      tot_stake += true_stake;
      tot_weight += weight;
      var vinfo = begin_cell()
        .store_uint(adnl_addr ? 0x73 : 0x53, 8)  ;; validator_addr#73 or validator#53
        .store_uint(0x8e81278a, 32)    ;; ed25519_pubkey#8e81278a
        .store_uint(pubkey, 256)       ;; pubkey:bits256
        .store_uint(weight, 64);       ;; weight:uint64
      if (adnl_addr) {
        vinfo~store_uint(adnl_addr, 256);  ;; adnl_addr:bits256
      }
      vset~udict_set_builder(16, i, vinfo);
      frozen~udict_set_builder(256, pubkey, begin_cell()
        .store_uint(src_addr, 256)
        .store_uint(weight, 64)
        .store_grams(true_stake)
        .store_int(false, 1));
    }
    if (stake) {
      ;; non-zero unused part of the stake, credit to the source address
      credits~credit_to(src_addr, stake);
    }
    i += 1;
  } until (l.null?());
  throw_unless(49, tot_stake == best_stake);
  return (credits, vset, tot_weight, frozen, tot_stake, m); 
}

int conduct_elections(ds, elect, credits) impure {
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  if (now() < elect_close) {
    ;; elections not finished yet
    return false;
  }
  if (config_param(0).null?()) {
    ;; no configuration smart contract to send result to
    return postpone_elections();
  }
  var cs = config_param(17).begin_parse();
  min_stake = cs~load_grams();
  var max_stake = cs~load_grams();
  var min_total_stake = cs~load_grams();
  var max_stake_factor = cs~load_uint(32);
  cs.end_parse();
  if (total_stake < min_total_stake) {
    ;; insufficient total stake, postpone elections
    return postpone_elections();
  }
  if (failed) {
    ;; do not retry failed elections until new stakes arrive
    return postpone_elections();
  }
  if (finished) {
    ;; elections finished
    return false;
  }
  (credits, var vdict, var total_weight, var frozen, var total_stakes, var cnt) = try_elect(credits, members, min_stake, max_stake, min_total_stake, max_stake_factor);
  ;; pack elections; if cnt==0, set failed=true, finished=false.
  failed = (cnt == 0);
  finished = ~ failed;
  elect = pack_elect(elect_at, elect_close, min_stake, total_stake, members, failed, finished);
  ifnot (cnt) {
    ;; elections failed, set elect_failed to true
    set_data(begin_cell().store_dict(elect).store_dict(credits).store_slice(ds).end_cell());
    return postpone_elections();
  }
  ;; serialize a query to the configuration smart contract
  ;; to install the computed validator set as the next validator set
  var (elect_for, elect_begin_before, elect_end_before, stake_held) = get_validator_conf();
  var start = max(now() + elect_end_before - 60, elect_at);
  var main_validators = config_param(16).begin_parse().skip_bits(16).preload_uint(16);
  var vset = begin_cell()
    .store_uint(0x12, 8)      ;; validators_ext#12
    .store_uint(start, 32)    ;; utime_since:uint32
    .store_uint(start + elect_for, 32)  ;; utime_until:uint32
    .store_uint(cnt, 16)      ;; total:(## 16) 
    .store_uint(min(cnt, main_validators), 16)  ;; main:(## 16)
    .store_uint(total_weight, 64)      ;; total_weight:uint64
    .store_dict(vdict)        ;; list:(HashmapE 16 ValidatorDescr)
  .end_cell();
  var config_addr = config_param(0).begin_parse().preload_uint(256);
  send_validator_set_to_config(config_addr, vset, elect_at);
  ;; add frozen to the dictionary of past elections
  var past_elections = ds~load_dict();
  past_elections~udict_set_builder(32, elect_at, pack_past_election(
    start + elect_for + stake_held, stake_held, vset.cell_hash(),
    frozen, total_stakes, 0, null()));
  ;; store credits and frozen until end
  set_data(begin_cell()
    .store_dict(elect)
    .store_dict(credits)
    .store_dict(past_elections)
    .store_slice(ds)
  .end_cell());
  return true;
}

int update_active_vset_id() impure {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var cur_hash = config_param(34).cell_hash();
  if (cur_hash == active_hash) {
    ;; validator set unchanged
    return false;
  }
  if (active_id) {
    ;; active_id becomes inactive
    var (fs, f) = past_elections.udict_get?(32, active_id);
    if (f) {
      ;; adjust unfreeze time of this validator set
      var unfreeze_time = fs~load_uint(32);
      var fs0 = fs;
      var (stake_held, hash) = (fs~load_uint(32), fs~load_uint(256));
      throw_unless(57, hash == active_hash);
      unfreeze_time = now() + stake_held;
      past_elections~udict_set_builder(32, active_id, begin_cell()
        .store_uint(unfreeze_time, 32)
        .store_slice(fs0));
    }
  }
  ;; look up new active_id by hash
  var id = -1;
  do {
    (id, var fs, var f) = past_elections.udict_get_next?(32, id);
    if (f) {
      var (tm, hash) = (fs~load_uint(64), fs~load_uint(256));
      if (hash == cur_hash) {
        ;; parse more of this record
        var (dict, total_stake, bonuses) = (fs~load_dict(), fs~load_grams(), fs~load_grams());
        ;; transfer 1/8 of accumulated everybody's grams to this validator set as bonuses
        var amount = (grams >> 3);
        grams -= amount;
        bonuses += amount;
        ;; serialize back
        past_elections~udict_set_builder(32, id, begin_cell()
          .store_uint(tm, 64)
          .store_uint(hash, 256)
          .store_dict(dict)
          .store_grams(total_stake)
          .store_grams(bonuses)
          .store_slice(fs));
        ;; found
        f = false;
      }
    }
  } until (~ f);
  active_id = (id.null?() ? 0 : id);
  active_hash = cur_hash;
  store_data(elect, credits, past_elections, grams, active_id, active_hash);
  return true;
}

int cell_hash_eq?(cell vset, int expected_vset_hash) inline_ref {
  return vset.null?() ? false : cell_hash(vset) == expected_vset_hash;
}

int validator_set_installed(ds, elect, credits) impure {
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  ifnot (finished) {
    ;; elections not finished yet
    return false;
  }
  var past_elections = ds~load_dict();
  var (fs, f) = past_elections.udict_get?(32, elect_at);
  ifnot (f) {
    ;; no election data in dictionary
    return false;
  }
  ;; recover validator set hash
  var vset_hash = fs.skip_bits(64).preload_uint(256);
  if (config_param(34).cell_hash_eq?(vset_hash) | config_param(36).cell_hash_eq?(vset_hash)) {
    ;; this validator set has been installed, forget elections
    set_data(begin_cell()
      .store_int(false, 1)   ;; forget current elections
      .store_dict(credits)
      .store_dict(past_elections)
      .store_slice(ds)
    .end_cell());
    update_active_vset_id();
    return true;
  }
  return false;
}

int check_unfreeze() impure {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  int id = -1;
  do {
    (id, var fs, var f) = past_elections.udict_get_next?(32, id);
    if (f) {
      var unfreeze_at = fs~load_uint(32);
      if ((unfreeze_at <= now()) & (id != active_id)) {
        ;; unfreeze!
        (credits, past_elections, var unused_prizes) = unfreeze_all(credits, past_elections, id);
        grams += unused_prizes;
        ;; unfreeze only one at time, exit loop
        store_data(elect, credits, past_elections, grams, active_id, active_hash);
        ;; exit loop
        f = false;
      }
    }
  } until (~ f);
  return ~ id.null?();
}

int announce_new_elections(ds, elect, credits) {
  var next_vset = config_param(36);   ;; next validator set
  ifnot (next_vset.null?()) {
    ;; next validator set exists, no elections needed
    return false;
  }
  var elector_addr = config_param(1).begin_parse().preload_uint(256);
  var (my_wc, my_addr) = my_address().parse_std_addr();
  if ((my_wc + 1) | (my_addr != elector_addr)) {
    ;; this smart contract is not the elections smart contract anymore, no new elections
    return false;
  }
  var cur_vset = config_param(34);  ;; current validator set
  if (cur_vset.null?()) {
    return false;
  }
  var (elect_for, elect_begin_before, elect_end_before, stake_held) = get_validator_conf();
  var cur_valid_until = cur_vset.begin_parse().skip_bits(8 + 32).preload_uint(32);
  var t = now();
  var t0 = cur_valid_until - elect_begin_before;
  if (t < t0) {
    ;; too early for the next elections
    return false;
  }
  ;; less than elect_before_begin seconds left, create new elections
  if (t - t0 < 60) {
    ;; pretend that the elections started at t0
    t = t0;
  }
  ;; get stake parameters
  (_, var min_stake) = config_param(17).begin_parse().load_grams();
  ;; announce new elections
  var elect_at = t + elect_begin_before;
  ;; elect_at~dump();
  var elect_close = elect_at - elect_end_before;
  elect = pack_elect(elect_at, elect_close, min_stake, 0, new_dict(), false, false);
  set_data(begin_cell().store_dict(elect).store_dict(credits).store_slice(ds).end_cell());
  return true;
}

() run_ticktock(int is_tock) impure {
  ;; check whether an election is being conducted
  var ds = get_data().begin_parse();
  var (elect, credits) = (ds~load_dict(), ds~load_dict());
  ifnot (elect.null?()) {
    ;; have an active election
    throw_if(0, conduct_elections(ds, elect, credits));  ;; elections conducted, exit
    throw_if(0, validator_set_installed(ds, elect, credits));  ;; validator set installed, current elections removed
  } else {
    throw_if(0, announce_new_elections(ds, elect, credits));  ;; new elections announced, exit
  }
  throw_if(0, update_active_vset_id());  ;; active validator set id updated, exit
  check_unfreeze();
}

;; Get methods

;; returns active election id or 0
int active_election_id() method_id {
  var elect = get_data().begin_parse().preload_dict();
  return elect.null?() ? 0 : elect.begin_parse().preload_uint(32);
}

;; checks whether a public key participates in current elections
int participates_in(int validator_pubkey) method_id {
  var elect = get_data().begin_parse().preload_dict();
  if (elect.null?()) {
    return 0;
  }
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  var (mem, found) = members.udict_get?(256, validator_pubkey);
  return found ? mem~load_grams() : 0;
}

;; returns the list of all participants of current elections with their stakes
_ participant_list() method_id {
  var elect = get_data().begin_parse().preload_dict();
  if (elect.null?()) {
    return nil;
  }
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  var l = nil;
  var id = (1 << 255) + ((1 << 255) - 1);
  do {
    (id, var fs, var f) = members.udict_get_prev?(256, id);
    if (f) {
      l = cons([id, fs~load_grams()], l);
    }
  } until (~ f);
  return l;
}

;; returns the list of all participants of current elections with their data
_ participant_list_extended() method_id {
  var elect = get_data().begin_parse().preload_dict();
  if (elect.null?()) {
    return (0, 0, 0, 0, nil, 0, 0);
  }
  var (elect_at, elect_close, min_stake, total_stake, members, failed, finished) = elect.unpack_elect();
  var l = nil;
  var id = (1 << 255) + ((1 << 255) - 1);
  do {
    (id, var cs, var f) = members.udict_get_prev?(256, id);
    if (f) {
      var (stake, time, max_factor, addr, adnl_addr) = (cs~load_grams(), cs~load_uint(32), cs~load_uint(32), cs~load_uint(256), cs~load_uint(256));
      cs.end_parse();
      l = cons([id, [stake, max_factor, addr, adnl_addr]], l);
    }
  } until (~ f);
  return (elect_at, elect_close, min_stake, total_stake, l, failed, finished);
}

;; computes the return stake
int compute_returned_stake(int wallet_addr) method_id {
  var cs = get_data().begin_parse();
  (_, var credits) = (cs~load_dict(), cs~load_dict());
  var (val, f) = credits.udict_get?(256, wallet_addr);
  return f ? val~load_grams() : 0;
}

;; returns the list of past election ids
tuple past_election_ids() method_id {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var id = (1 << 32);
  var list = null();
  do {
    (id, var fs, var f) = past_elections.udict_get_prev?(32, id);
    if (f) {
      list = cons(id, list);
    }
  } until (~ f);
  return list;
}

tuple past_elections() method_id {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var id = (1 << 32);
  var list = null();
  do {
    (id, var fs, var found) = past_elections.udict_get_prev?(32, id);
    if (found) {
      list = cons([id, unpack_past_election(fs)], list);
    }
  } until (~ found);
  return list;
}

tuple past_elections_list() method_id {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var id = (1 << 32);
  var list = null();
  do {
    (id, var fs, var found) = past_elections.udict_get_prev?(32, id);
    if (found) {
      var (unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints) = unpack_past_election(fs);
      list = cons([id, unfreeze_at, vset_hash, stake_held], list);
    }
  } until (~ found);
  return list;
}

_ complete_unpack_complaint(slice cs) inline_ref {
  var (complaint, voters, vset_id, weight_remaining) = cs.unpack_complaint_status();
  var voters_list = null();
  var voter_id = (1 << 32);
  do {
    (voter_id, _, var f) = voters.udict_get_prev?(16, voter_id);
    if (f) {
      voters_list = cons(voter_id, voters_list);
    }
  } until (~ f);
  return [[complaint.begin_parse().unpack_complaint()], voters_list, vset_id, weight_remaining];
}

cell get_past_complaints(int election_id) inline_ref method_id {
  var (elect, credits, past_elections, grams, active_id, active_hash) = load_data();
  var (fs, found?) = past_elections.udict_get?(32, election_id);
  ifnot (found?) {
    return null();
  }
  var (unfreeze_at, stake_held, vset_hash, frozen_dict, total_stake, bonuses, complaints) = unpack_past_election(fs);
  return complaints;
}

_ show_complaint(int election_id, int chash) method_id {
  var complaints = get_past_complaints(election_id);
  var (cs, found) = complaints.udict_get?(256, chash);
  return found ? complete_unpack_complaint(cs) : null();
}

tuple list_complaints(int election_id) method_id {
  var complaints = get_past_complaints(election_id);
  int id = (1 << 255) + ((1 << 255) - 1);
  var list = null();
  do {
    (id, var cs, var found?) = complaints.udict_get_prev?(256, id);
    if (found?) {
      list = cons(pair(id, complete_unpack_complaint(cs)), list);
    }
  } until (~ found?);
  return list;
}

int complaint_storage_price(int bits, int refs, int expire_in) method_id {
  ;; compute complaint storage/creation price
  var (deposit, bit_price, cell_price) = get_complaint_prices();
  var pps = (bits + 1024) * bit_price + (refs + 2) * cell_price;
  var paid = pps * expire_in + deposit;
  return paid + (1 << 30);
}
