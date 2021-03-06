/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "elf/ai/tree_search/tree_search_options.h"
#include "elf/utils/json_utils.h"

#include "base/board.h"
#include "base/common.h"

using json = nlohmann::json;

enum ClientType {
  CLIENT_INVALID,
  CLIENT_SELFPLAY_ONLY,
  CLIENT_EVAL_THEN_SELFPLAY
};

struct ClientCtrl {
  ClientType client_type = CLIENT_SELFPLAY_ONLY;
  // -1 means to use all the threads.
  int num_game_thread_used = -1;

  float black_resign_thres = 0.0;
  float white_resign_thres = 0.0;
  float never_resign_prob = 0.0;

  bool player_swap = false;
  bool async = false;

  void setJsonFields(json& j) const {
    JSON_SAVE(j, client_type);
    JSON_SAVE(j, num_game_thread_used);
    JSON_SAVE(j, black_resign_thres);
    JSON_SAVE(j, white_resign_thres);
    JSON_SAVE(j, never_resign_prob);
    JSON_SAVE(j, player_swap);
    JSON_SAVE(j, async);
  }
  static ClientCtrl createFromJson(
      const json& j,
      bool player_swap_optional = false) {
    ClientCtrl ctrl;
    JSON_LOAD(ctrl, j, client_type);
    JSON_LOAD(ctrl, j, num_game_thread_used);
    JSON_LOAD(ctrl, j, black_resign_thres);
    JSON_LOAD(ctrl, j, white_resign_thres);
    JSON_LOAD(ctrl, j, never_resign_prob);
    // For backward compatibility.
    if (player_swap_optional) {
      JSON_LOAD_OPTIONAL(ctrl, j, player_swap);
    } else {
      JSON_LOAD(ctrl, j, player_swap);
    }
    JSON_LOAD_OPTIONAL(ctrl, j, async);
    return ctrl;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[client=" << client_type << "][async=" << async << "]"
       << "[#th=" << num_game_thread_used << "]"
       << "[b_res_th=" << black_resign_thres
       << "][w_res_th=" << white_resign_thres << "][swap=" << player_swap
       << "][never_res_pr=" << never_resign_prob << "]";
    return ss.str();
  }

  friend bool operator==(const ClientCtrl& c1, const ClientCtrl& c2) {
    return c1.client_type == c2.client_type &&
        c1.num_game_thread_used == c2.num_game_thread_used &&
        c1.black_resign_thres == c2.black_resign_thres &&
        c1.white_resign_thres == c2.white_resign_thres &&
        c1.never_resign_prob == c2.never_resign_prob &&
        c1.player_swap == c2.player_swap && c1.async == c2.async;
  }
  friend bool operator!=(const ClientCtrl& c1, const ClientCtrl& c2) {
    return !(c1 == c2);
  }
};

struct ModelPair {
  int64_t black_ver = -1;
  int64_t white_ver = -1;
  elf::ai::tree_search::TSOptions mcts_opt;

  bool wait() const {
    return black_ver < 0;
  }
  void set_wait() {
    black_ver = white_ver = -1;
  }

  bool is_selfplay() const {
    return black_ver >= 0 && white_ver == -1;
  }

  std::string info() const {
    std::stringstream ss;
    if (wait())
      ss << "[wait]";
    else if (is_selfplay())
      ss << "[selfplay=" << black_ver << "]";
    else
      ss << "[b=" << black_ver << "][w=" << white_ver << "]";
    ss << mcts_opt.info();
    return ss.str();
  }

  friend bool operator==(const ModelPair& p1, const ModelPair& p2) {
    return p1.black_ver == p2.black_ver && p1.white_ver == p2.white_ver &&
        p1.mcts_opt == p2.mcts_opt;
  }
  friend bool operator!=(const ModelPair& p1, const ModelPair& p2) {
    return !(p1 == p2);
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, black_ver);
    JSON_SAVE(j, white_ver);
    JSON_SAVE_OBJ(j, mcts_opt);
  }

  static ModelPair createFromJson(const json& j) {
    ModelPair p;
    // cout << "extract black_Ver" << endl;
    JSON_LOAD(p, j, black_ver);
    // cout << "extract white_Ver" << endl;
    JSON_LOAD(p, j, white_ver);
    // cout << "extract MCTS" << endl;
    JSON_LOAD_OBJ(p, j, mcts_opt);
    // cout << "extract MCTS complete" << endl;
    return p;
  }
};

namespace std {
template <>
struct hash<ModelPair> {
  typedef ModelPair argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    result_type const h1(std::hash<int64_t>{}(s.black_ver));
    result_type const h2(std::hash<int64_t>{}(s.white_ver));
    result_type const h3(
        std::hash<elf::ai::tree_search::TSOptions>{}(s.mcts_opt));
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};
} // namespace std

struct MsgVersion {
  int64_t model_ver;
  MsgVersion(int ver = -1) : model_ver(ver) {}
};

enum RestartReply {
  NO_OP,
  ONLY_WAIT,
  UPDATE_REQUEST_ONLY,
  UPDATE_MODEL,
  UPDATE_MODEL_ASYNC
};

struct MsgRestart {
  RestartReply result;
  int game_idx;
  MsgRestart(RestartReply res = NO_OP, int game_idx = -1)
      : result(res), game_idx(game_idx) {}
};

struct MsgRequest {
  ModelPair vers;
  ClientCtrl client_ctrl;

  void setJsonFields(json& j) const {
    JSON_SAVE_OBJ(j, vers);
    JSON_SAVE_OBJ(j, client_ctrl);
  }

  static MsgRequest createFromJson(const json& j) {
    MsgRequest request;
    JSON_LOAD_OBJ(request, j, vers);
    JSON_LOAD_OBJ_ARGS(request, j, client_ctrl, request.vers.is_selfplay());
    return request;
  }

  std::string setJsonFields() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  std::string info() const {
    std::stringstream ss;
    ss << client_ctrl.info() << vers.info();
    return ss.str();
  }

  friend bool operator==(const MsgRequest& m1, const MsgRequest& m2) {
    return m1.vers == m2.vers && m1.client_ctrl == m2.client_ctrl;
  }

  friend bool operator!=(const MsgRequest& m1, const MsgRequest& m2) {
    return !(m1 == m2);
  }
};

struct MsgRequestSeq {
  int64_t seq = -1;
  MsgRequest request;

  void setJsonFields(json& j) const {
    JSON_SAVE_OBJ(j, request);
    JSON_SAVE(j, seq);
  }

  static MsgRequestSeq createFromJson(const json& j) {
    MsgRequestSeq s;
    JSON_LOAD_OBJ(s, j, request);
    JSON_LOAD(s, j, seq);
    return s;
  }
  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[seq=" << seq << "]" << request.info();
    return ss.str();
  }
};

struct CoordRecord {
  unsigned char prob[BOUND_COORD];
};

struct MsgResult {
  int num_move = 0;
  float reward = 0.0;
  bool black_never_resign = false;
  bool white_never_resign = false;
  // Whether this replay is generated by mutliple models.
  std::vector<int64_t> using_models;
  std::string content;
  std::vector<CoordRecord> policies;
  std::vector<float> values;

  std::string info() const {
    std::stringstream ss;
    ss << "[num_move=" << num_move << "]";
    ss << "[models=";
    for (const auto& i : using_models)
      ss << i << ", ";
    ss << "]";
    ss << "[reward=" << reward << "][b_no_res=" << black_never_resign
       << "][w_no_res=" << white_never_resign
       << "] len(content)=" << content.size();
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, num_move);
    JSON_SAVE(j, reward);
    JSON_SAVE(j, black_never_resign);
    JSON_SAVE(j, white_never_resign);
    JSON_SAVE(j, using_models);
    JSON_SAVE(j, content);

    for (size_t i = 0; i < policies.size(); i++) {
      json j1;
      for (unsigned char c : policies[i].prob) {
        j1.push_back(c);
      }
      j["policies"].push_back(j1);
    }

    JSON_SAVE(j, values);
  }

  static MsgResult createFromJson(const json& j) {
    MsgResult res;

    // cout << "extract num_moves" << endl;
    JSON_LOAD(res, j, num_move);
    JSON_LOAD(res, j, reward);
    JSON_LOAD(res, j, content);
    JSON_LOAD(res, j, black_never_resign);
    JSON_LOAD(res, j, white_never_resign);
    JSON_LOAD_VEC_OPTIONAL(res, j, using_models);
    JSON_LOAD_VEC(res, j, values);

    if (j.find("policies") != j.end()) {
      // cout << "extract policies" << endl;
      size_t num_policies = j["policies"].size();
      // cout << "Content: " << r.content << endl;
      //
      for (size_t i = 0; i < num_policies; i++) {
        json j1 = j["policies"][i];
        res.policies.emplace_back();
        for (size_t k = 0; k < j1.size(); k++) {
          res.policies.back().prob[k] = j1[k];
        }
      }
      // cout << "extract policies complete: " << num_policies << endl;
    }
    // cout << "#policies: " << num_policies << " #entries: " << total_entries
    //     << ", entries/policy: " << (float)(total_entries) / num_policies <<
    //     endl;
    return res;
  }
};

struct Record {
  MsgRequest request;
  MsgResult result;

  uint64_t timestamp = 0;
  uint64_t thread_id = 0;
  int seq = 0;
  float pri = 0.0;
  bool offline = false;

  std::string info() const {
    std::stringstream ss;
    ss << "[t=" << timestamp << "][id=" << thread_id << "][seq=" << seq
       << "][pri=" << pri << "][offline=" << offline << "]" << std::endl;
    ss << request.info() << std::endl;
    ss << result.info() << std::endl;
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE_OBJ(j, request);
    JSON_SAVE_OBJ(j, result);
    JSON_SAVE(j, timestamp);
    JSON_SAVE(j, thread_id);
    JSON_SAVE(j, seq);
    JSON_SAVE(j, pri);
    JSON_SAVE(j, offline);
  }

  static Record createFromJson(const json& j) {
    Record r;

    JSON_LOAD_OBJ(r, j, request);
    JSON_LOAD_OBJ(r, j, result);
    JSON_LOAD(r, j, timestamp);
    JSON_LOAD(r, j, thread_id);
    JSON_LOAD(r, j, seq);
    JSON_LOAD(r, j, pri);
    JSON_LOAD_OPTIONAL(r, j, offline);
    return r;
  }

  // Extra serialization.
  static std::vector<Record> createBatchFromJson(const std::string& json_str) {
    auto j = json::parse(json_str);
    // cout << "from json_batch" << endl;
    std::vector<Record> records;
    for (size_t i = 0; i < j.size(); ++i) {
      try {
        records.push_back(createFromJson(j[i]));
      } catch (...) {
      }
    }
    return records;
  }

  static bool loadBatchFromJsonFile(
      const std::string& f,
      std::vector<Record>* records) {
    assert(records != nullptr);

    try {
      std::ifstream iFile(f.c_str());
      iFile.seekg(0, std::ios::end);
      size_t size = iFile.tellg();
      std::string buffer(size, ' ');
      iFile.seekg(0);
      iFile.read(&buffer[0], size);

      *records = createBatchFromJson(buffer);
      return true;
    } catch (...) {
      return false;
    }
  }

  static std::string dumpBatchJsonString(
      std::vector<Record>::const_iterator b,
      std::vector<Record>::const_iterator e) {
    json j;
    for (auto it = b; it != e; ++it) {
      json j1;
      it->setJsonFields(j1);
      j.push_back(j1);
    }
    return j.dump();
  }
};

struct ThreadState {
  int thread_id = -1;
  // Which game we have played.
  int seq = 0;
  // Which move we have proceeded.
  int move_idx = 0;

  int64_t black = -1;
  int64_t white = -1;

  void setJsonFields(json& j) const {
    JSON_SAVE(j, thread_id);
    JSON_SAVE(j, seq);
    JSON_SAVE(j, move_idx);
    JSON_SAVE(j, black);
    JSON_SAVE(j, white);
  }

  static ThreadState createFromJson(const json& j) {
    ThreadState state;
    JSON_LOAD(state, j, thread_id);
    JSON_LOAD(state, j, seq);
    JSON_LOAD(state, j, move_idx);
    JSON_LOAD(state, j, black);
    JSON_LOAD(state, j, white);
    return state;
  }

  friend bool operator==(const ThreadState& t1, const ThreadState& t2) {
    return t1.thread_id == t2.thread_id && t1.seq == t2.seq &&
        t1.move_idx == t2.move_idx && t1.black == t2.black &&
        t1.white == t2.white;
  }

  friend bool operator!=(const ThreadState& t1, const ThreadState& t2) {
    return !(t1 == t2);
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[th_id=" << thread_id << "][seq=" << seq << "][mv_idx=" << move_idx
       << "]"
       << "[black=" << black << "][white=" << white << "]";
    return ss.str();
  }
};

struct Records {
  std::string identity;
  std::unordered_map<int, ThreadState> states;
  std::vector<Record> records;

  Records() {}
  Records(const std::string& id) : identity(id) {}

  void clear() {
    states.clear();
    records.clear();
  }

  void addRecord(Record&& r) {
    records.emplace_back(r);
  }

  bool isRecordEmpty() const {
    return records.empty();
  }

  void updateState(const ThreadState& ts) {
    states[ts.thread_id] = ts;
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, identity);
    for (const auto& t : states) {
      json jj;
      t.second.setJsonFields(jj);
      j["states"].push_back(jj);
    }

    for (const Record& r : records) {
      json j1;
      r.setJsonFields(j1);
      j["records"].push_back(j1);
    }
  }

  static Records createFromJson(const json& j) {
    Records rs;
    JSON_LOAD(rs, j, identity);
    if (j.find("states") != j.end()) {
      for (size_t i = 0; i < j["states"].size(); ++i) {
        ThreadState t = ThreadState::createFromJson(j["states"][i]);
        rs.states[t.thread_id] = t;
      }
    }

    if (j.find("records") != j.end()) {
      // cout << "from json_batch" << endl;
      for (size_t i = 0; i < j["records"].size(); ++i) {
        rs.records.push_back(Record::createFromJson(j["records"][i]));
      }
    }
    return rs;
  }

  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  static Records createFromJsonString(const std::string& s) {
    return createFromJson(json::parse(s));
  }
};
