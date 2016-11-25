#include<iostream>
#include<sstream>
#include<fstream>
#include <algorithm>

#include "UFEConfigFrame.h"
#include "UFEError.h"

using namespace std;

UFEConfigFrame::UFEConfigFrame(Json::Value c) {
  LoadConfigFrame(c);
}

void UFEConfigFrame::LoadConfigFrameFromJsonFile(std::string file) {
  Json::Value json_doc;
  this->LoadJsonFile(file, json_doc);
  this->LoadConfigFrame(json_doc);
}

void UFEConfigFrame::LoadUserConfigFromJsonFile(std::string file, int dev) {
  Json::Value json_doc;
  this->LoadJsonFile(file, json_doc);
  this->LoadUserConfig(json_doc, dev);
}

void UFEConfigFrame::LoadDirectParamFromJsonFile(std::string file, uint16_t &par) {
  Json::Value json_doc;
  this->LoadJsonFile(file, json_doc);
  this->LoadDirectParam(json_doc, par);
}

void UFEConfigFrame::LoadReadoutParamFromJsonFile(std::string file, uint16_t &par) {
  Json::Value json_doc;
  this->LoadJsonFile(file, json_doc);
  this->LoadReadoutParam(json_doc, par);
}

void UFEConfigFrame::LoadJsonFile(std::string file, Json::Value &json_doc) {
  stringstream buffer;
  ifstream config_file(file);
  if (!config_file) {
    stringstream ss;
    ss << "Can not open file " << file;
    throw UFEError( ss.str(),
                    "void UFEConfigFrame::LoadJsonFile(std::string, Json::Value&)" ,
                    UFEError::FATAL);
  }

  buffer << config_file.rdbuf();

  Json::Reader reader;
  bool parsingSuccessful = reader.parse(buffer, json_doc);

  if ( !parsingSuccessful ) {
    stringstream ss;
    ss << "Failed to parse configuration description. ***\n***"
       << reader.getFormattedErrorMessages();
    throw UFEError( ss.str(),
                    "void UFEConfigFrame::LoadJsonFile(std::string, Json::Value&)" ,
                    UFEError::FATAL);
  }
}

void UFEConfigFrame::LoadConfigFrame(const Json::Value &conf) {
  try {
    SET_MEMBER_STRING(this, conf, Name, 1)
    SET_MEMBER_DOUBLE_ASSTRING(this, conf, Version, 1)
    SET_MEMBER_DOUBLE_ASSTRING(this, conf, MinFpgaVersion, 1)
    SET_MEMBER_INT(this, conf, DeviceMemorySize, 1)

    if (conf.isMember("FirmwareVIds"))
      for (auto const& xFV : conf["FirmwareVIds"]) {
//         cout << xFV << endl;
        FirmwareVId fv;
        fv << xFV;
        FirmwareVIds_.push_back(fv);
      }

    if (conf.isMember("HardwareVIds"))
      for (auto const& xHV : conf["HardwareVIds"]) {
//         cout << xFV << endl;
        HardwareVId hv;
        hv << xHV;
        HardwareVIds_.push_back(hv);
      }

    if (conf.isMember("Board")) {
      this->GetBoardConfig(conf["Board"]);
    }

    if (conf.isMember("Children")) {
      for (auto const& device : conf["Children"]) {
        if (device["Name"].asString() == "FPGA")
          this->GetFpgaConfig(device["Children"]);
        else if (device["Name"].asString() == "ASICS")
          this->GetAsicConfig(device["Children"]);
      }
    }
  } catch (UFEError &e) {
    cerr << e.GetDescription() << endl;
    cerr << e.GetLocation() << endl;
  }
}

void UFEConfigFrame::GetFpgaConfig(const Json::Value &conf) {
  if (conf.isArray())
    if (conf.size() == 2) {
      for (auto const & ch : conf) {
        if (ch["Name"] == "ASIC")
          this->GetFpgaAsicConfig(ch);

        if (ch["Name"] == "GlobalControl")
          this->GetFpgaGlobalControl(ch["Children"]);
      }

      return;
  }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetFpgaConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetFpgaGlobalControl(const Json::Value &conf) {
  if (conf.isArray()) {
    for (auto const & gc_var : conf) {
//       cout << gc_var << endl;
      Variable v;
      v << gc_var;
      Fpga_.GlobalControlParameters_.push_back(v);
    }

    return;
  }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetFpgaGlobalControl(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetFpgaAsicConfig(const Json::Value &conf) {
  if ( conf["Children"].isArray() && conf.isMember("NumInstances") ) {
    int nAsics = conf["NumInstances"].asInt();
    Fpga_.Asics_.resize(nAsics);
    for (int xAsic=0; xAsic<nAsics;++xAsic)
    for (auto const & ch : conf["Children"]) {
      if (ch["Name"] == "Channels") {
        int nCh = ch["Children"][0]["NumInstances"].asInt();
        Fpga_.Asics_[xAsic].Channels_.resize(nCh);
        for (int xCh=0; xCh<nCh; ++xCh)
          for (auto const& channel_var : ch["Children"][0]["Children"]) {
//               if (xCh==0 && xAsic==0) cout << channel_var << endl;
              Variable v;
              v << channel_var;
//               stringstream ss;
//               ss << v.Name_ << "_asic" << xAsic << "_ch" << xCh;
//               v.Name_ = ss.str();
              if (v.MemoryLayout_.Absolute_)
                v.MemoryLayout_.Index_ += (xAsic*nCh + xCh)*v.BitSize_;
              else
                v.MemoryLayout_.Index_ += (xAsic*97 + xCh)*v.BitSize_;

              Fpga_.Asics_[xAsic].Channels_[xCh].ChannelParameters_.push_back(v);
          }
      }

      if (ch["Name"] == "GlobalControl") {
        for (auto const& gc_var : ch["Children"]) {
//           cout << gc_var << endl;
          Variable v;
          v << gc_var;

          if (v.MemoryLayout_.Index_ < 0)
            v.MemoryLayout_.Index_ = v.MemoryLayout_.AbsoluteIndexes_[xAsic];
          else v.MemoryLayout_.Index_ += xAsic*97;                                               // !!!!!!!!!!!!!!!!!!!

          Fpga_.Asics_[xAsic].GlobalControlParameters_.push_back(v);
        }
      }
    }

    return;
  }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetFpgaChannelsConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetAsicConfig(const Json::Value &conf) {
  if (conf.isArray())
    if (conf.size() == 1)
      if (conf[0]["Name"].asString() == "ASIC") {
        if (conf[0]["Children"].isArray()) {
          for (auto const & ch : conf[0]["Children"]) {
            if (ch["Name"] == "Channels")
              this->GetAsicChannelsConfig(ch["Children"]);
            else if (ch["Name"] == "PowerModes")
              this->GetAsicPowerModesConfig(ch["Children"]);
            else if (ch["Name"] == "GlobalControl" )
              this->GetAsicGlobalControlConfig(ch["Children"]);
            }

            return;
          }
      }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetAsicSConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetAsicGlobalControlConfig(const Json::Value &conf) {
  if (conf.isArray()) {
    for (auto const & gc_var : conf) {
//       cout << gc_var << endl;
      Variable v;
      v << gc_var;
      Asic_.GlobalControlParameters_.push_back(v);
    }

    return;
  }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetAsicGlobalControlConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetAsicPowerModesConfig(const Json::Value &conf) {
  if (conf.isArray()) {
    for (auto const & pm_var : conf) {
//       cout << pm_var << endl;
      Variable v;
      v << pm_var;
      Asic_.PowerModesParameters_.push_back(v);
    }

    return;
  }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetAsicPowerModesConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetAsicChannelsConfig(const Json::Value &conf) {
  if (conf.isArray())
    if (conf.size() == 1)
      if (conf[0]["Name"] == "Channel") {
        int nCh = conf[0]["NumInstances"].asInt();
        Asic_.Channels_.resize( nCh );
        if (conf[0]["Children"].isArray()) {
          for (int xCh=0; xCh<nCh; ++xCh)
            for (auto const& channel_var : conf[0]["Children"]) {
//               if (xCh==0) cout << channel_var << endl;
              Variable v;
              v << channel_var;

              if (v.MemoryLayout_.Increment_ > 0)
                v.MemoryLayout_.Index_ += xCh*v.MemoryLayout_.Increment_;
              else
                v.MemoryLayout_.Index_ += xCh*v.BitSize_;
//               stringstream ss;
//               ss << v.Name_ << "_ch" << xCh;
//               v.Name_ = ss.str();
              Asic_.Channels_[xCh].ChannelParameters_.push_back(v);
            }

          return;
        }
      }

  throw UFEError( "Unexpected structure of the configuration description.",
                  "UFEConfigFrame::GetAsicChannelsConfig(const Json::Value&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::GetBoardConfig(const Json::Value &conf) {
  if (conf.isMember("DirectParameters"))
    if (conf["DirectParameters"].isMember("Parameters"))
      for (auto const& dp_var : conf["DirectParameters"]["Parameters"]) {
//      cout << dp_var << endl;
        Variable v;
        v << dp_var;
        Board_.DirectParameters_.push_back(v);
      }

  if (conf.isMember("DataReadoutParameters"))
    if (conf["DataReadoutParameters"].isMember("Parameters"))
      for (auto const& drp_var : conf["DataReadoutParameters"]["Parameters"]) {
//         cout << drp_var << endl;
        Variable v;
        v << drp_var;
        Board_.DataReadoutParameters_.push_back(v);
      }

  if (conf.isMember("StatusParameters"))
    if (conf["StatusParameters"].isMember("Parameters"))
      for (auto const& sp_var : conf["StatusParameters"]["Parameters"]) {
//         cout << sp_var << endl;
        Variable v;
        v << sp_var;
        Board_.StatusParameters_.push_back(v);
      }
}

void UFEConfigFrame::GetAsicUserConfig(Json::Value u) {

  if (u.isMember("Children"))
  for (auto const& par : u["Children"]) {
    if (par["Name"] == "Channels") {
      for (auto const& ch : par["Children"]) {
        int xCh;
        stringstream ss;
        ss << (ch["Name"].asCString()+7);
        ss >> xCh;
//         cout << "ASIC ch" << xCh << endl;
        this->Asic_.Channels_[xCh].ChannelParameters_ << ch;
      }
    } else if (par["Name"] == "PowerModes") {
      this->Asic_.PowerModesParameters_ << par;
    } else if (par["Name"] == "GlobalControl") {
      this->Asic_.GlobalControlParameters_ << par;
    } else {
      throw UFEError( "Unexpected structure of the user configuration.",
                      "void UFEConfigFrame::GetAsicUserConfig(Json::Value)" ,
                      UFEError::FATAL);
    }
  }
}

void UFEConfigFrame::GetFpfaUserConfig(Json::Value u)  {
  if (u.isArray())
  for (auto const& fpga : u) {
    if (fpga["Name"] == "GlobalControl")
      this->Fpga_.GlobalControlParameters_ << fpga;

    if (fpga["Name"] == "ASIC0") {
      for (auto const& par : fpga["Children"]) {
        if (par["Name"] == "GlobalControl")
          this->Fpga_.Asics_[0].GlobalControlParameters_ << par;

        if (par["Name"] == "Channels") {
          for (auto const& ch : par["Children"]) {
            int xCh;
            stringstream ss;
            ss << (ch["Name"].asCString()+7);
            ss >> xCh;
//             cout << "FPGA ASIC0 ch" << xCh << endl;
            this->Fpga_.Asics_[0].Channels_[xCh].ChannelParameters_ << ch;
          }
        }
      }
    }

    if (fpga["Name"] == "ASIC1") {
      for (auto const& par : fpga["Children"]) {
        if (par["Name"] == "GlobalControl")
          this->Fpga_.Asics_[1].GlobalControlParameters_ << par;

        if (par["Name"] == "Channels") {
          for (auto const& ch : par["Children"]) {
            int xCh;
            stringstream ss;
            ss << (ch["Name"].asCString()+7);
            ss >> xCh;
//             cout << "FPGA ASIC1 ch" << xCh << endl;
            this->Fpga_.Asics_[1].Channels_[xCh].ChannelParameters_ << ch;
          }
        }
      }
    }

    if (fpga["Name"] == "ASIC2") {
      for (auto const& par : fpga["Children"]) {
        if (par["Name"] == "GlobalControl")
          this->Fpga_.Asics_[2].GlobalControlParameters_ << par;

        if (par["Name"] == "Channels") {
          for (auto const& ch : par["Children"]) {
            int xCh;
            stringstream ss;
            ss << (ch["Name"].asCString()+7);
            ss >> xCh;
//             cout << "FPGA ASIC2 ch" << xCh << endl;
            this->Fpga_.Asics_[2].Channels_[xCh].ChannelParameters_ << ch;
          }
        }
      }
    }
  }
}

void UFEConfigFrame::LoadUserConfig(const Json::Value &c, unsigned int dev) {
  if (dev > 3) {
    stringstream ss;
    ss << "Devise " << dev << " does not exist.";
    throw UFEError( ss.str(),
                    "void UFEConfigFrame::LoadUserConfig(Json::Value, unsigned int)" ,
                    UFEError::FATAL);
  }

  if (dev < 3) {
    for (auto const& u : c["Children"]) {
      if (u["Name"] == "ASICS") {
        if (u.isMember("Children")) {
          stringstream ss;
          ss << "ASIC" << dev;
          for (auto const& a : u["Children"])
            if (a["Name"] == ss.str()) {
              GetAsicUserConfig (a);
              return;
            }
        }
      }
    }
  } else if (dev == 3) {
    for (auto const& u : c["Children"]) {
      if (u["Name"] == "FPGA") {
        GetFpfaUserConfig(u["Children"]);
        return;
      }
    }
  }

  throw UFEError( "Unexpected structure of the user configuration.",
                  "void UFEConfigFrame::LoadUserConfig(Json::Value, unsigned int)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::LoadDirectParam(const Json::Value &c, uint16_t &par) {
  for (auto const& b : c["Children"])
    if (b["Name"] == "Board")
      for (auto const& dp : b["Children"])
        if (dp["Name"] == "DirectParam") {
//           cout << dp << endl;
          this->Board_.DirectParameters_ << dp;
//           cout << this->Board_.DirectParameters_ << endl;
          par = this->Buffer_.SetParams(this->Board_.DirectParameters_);
          return;
        }

  throw UFEError( "Unexpected structure of the user configuration.",
                  "void UFEConfigFrame::LoadDirectParam(const Json::Value&, uint16_t&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::LoadReadoutParam(const Json::Value &c, uint16_t &par) {
  for (auto const& b : c["Children"])
    if (b["Name"] == "Board")
      for (auto const& rp : b["Children"])
        if (rp["Name"] == "DataReadoutParam") {
//           cout << rp << endl;
          this->Board_.DataReadoutParameters_ << rp;
//           cout << this->Board_.DataReadoutParameters_ << endl;
          par = this->Buffer_.SetParams(this->Board_.DataReadoutParameters_);
          return;
        }

  throw UFEError( "Unexpected structure of the user configuration.",
                  "void UFEConfigFrame::LoadReadoutParam(const Json::Value&, uint16_t&)" ,
                  UFEError::FATAL);
}

void UFEConfigFrame::SetConfigBuffer(unsigned int dev) {
  vector<Variable> index;
  this->GetSortedList(index, dev);
  Buffer_.Reset();
  Buffer_.SetActualSize(index.back().MemoryLayout_.Index_ + index.back().BitSize_);
  if (dev < 3) {
    for (auto & v:index)
      Buffer_.SetBitField(v, 'r');
  } else if ( dev == 3) {
    for (auto & v:index)
      Buffer_.SetBitField(v, 'n');
  } else {
    stringstream ss;
    ss << "Devise " << dev << " does not exist.";
    throw UFEError( ss.str(),
                    "void UFEConfigFrame::LoadUserConfig(Json::Value, unsigned int)" ,
                    UFEError::FATAL);
  }
}

int UFEConfigFrame::GetSortedList(vector<Variable> &index, unsigned int dev) {
  if (dev < 3) {
    for (auto const & ch : this->Asic_.Channels_)
      for (auto const & var : ch.ChannelParameters_)
        index.push_back(var);

    for (auto const & var : this->Asic_.PowerModesParameters_)
      index.push_back(var);

    for (auto const & var : this->Asic_.GlobalControlParameters_)
      index.push_back(var);

  } else if ( dev == 3) {
    for (auto const & var : this->Fpga_.GlobalControlParameters_)
      index.push_back(var);

    for (int xAsic=0; xAsic<3; ++xAsic) {
      for (auto const & ch : this->Fpga_.Asics_[xAsic].Channels_)
        for (auto const & var : ch.ChannelParameters_)
          index.push_back(var);

      for (auto const & var : this->Fpga_.Asics_[xAsic].GlobalControlParameters_)
        index.push_back(var);

    }
  } else {
    stringstream ss;
    ss << "Devise " << dev << " does not exist.";
    throw UFEError( ss.str(),
                    "void UFEConfigFrame::LoadUserConfig(Json::Value, unsigned int)" ,
                    UFEError::FATAL);
  }

  std::sort(index.begin(), index.end(), varSorter_);
  return index.size();
}

void UFEConfigFrame::DumpBuffer() {
  Buffer_.Dump();
}

void UFEConfigFrame::DumpBufferToFile(ofstream &file) {
  Buffer_.DumpToFile(file);
}

void UFEConfigFrame::PrintBuffer() {
  Buffer_.Print();
}

