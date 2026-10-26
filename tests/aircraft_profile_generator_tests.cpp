#include "aircraft_profile.hpp"
#include "aircraft_profile_generator.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
std::string read(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary);check(bool(f),"Cannot read fixture: "+path.u8string());
    return std::string((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
}
void write(const std::filesystem::path& path,const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);out<<text;check(bool(out),"Cannot write fixture");
}
int main(int argc,char** argv) {
    try {
        check(argc==4,"Usage: generator_tests MODEL_PATH REFERENCE_CONFIG NEW_OUTPUT_DIRECTORY");
        const auto model=std::filesystem::u8path(argv[1]),reference=std::filesystem::u8path(argv[2]),directory=std::filesystem::u8path(argv[3]);
        check(!std::filesystem::exists(directory),"Use a new output directory");
        std::filesystem::create_directories(directory);
        std::string error,id;std::filesystem::path loaded;
        check(profile::load(reference,"A20N",error),error);const auto expected=profile::current;
        const auto good=read(reference/"a20n.json");
        auto load=[&](const auto& config,const auto& acf,const std::string& icao="A20N") {
            return profile::load_for_aircraft(config,acf,icao,loaded,error);
        };
        auto identify=[&](const auto& acf) {
            std::string value;check(profile::identify(acf,value,error),error);return value;
        };
        const auto original_id=identify(model);
        check(std::regex_match(original_id,std::regex(".+-[0-9a-f]{4}-[0-9a-f]{4}")),"ID format");
        check(original_id.rfind(model.stem().u8string()+"-",0)==0,"ACF stem missing from ID");
        const auto generated=directory/"generated";
        check(!load(generated,model,"A319"),"ACF/active ICAO mismatch accepted");
        check(!load(generated,model.parent_path()/"missing.acf"),"Missing source accepted");
        check(!std::filesystem::exists(generated),"Failed extraction published a profile");
        check(load(generated,model),error);
        check(loaded==generated/(original_id+".json"),"Generated file is not ID-named");
        check(!std::filesystem::exists(generated/"a20n.json"),"Base profile was generated");
        const auto actual=profile::current;
        double max_difference=0;
        for(int side=0;side<2;++side) {
            check(actual.nd_brightness[side]==expected.nd_brightness[side],"ND brightness differs");
            check(actual.pfd_brightness[side]==expected.pfd_brightness[side],"PFD brightness differs");
            for(int mode=0;mode<2;++mode)for(int i=0;i<4;++i) {
                check(actual.nd_uv[mode][side][i]==expected.nd_uv[mode][side][i],"ND UV differs");
                check(actual.pfd_uv[mode][side][i]==expected.pfd_uv[mode][side][i],"PFD UV differs");
            }
            for(int corner=0;corner<4;++corner)for(int axis=0;axis<3;++axis) {
                const double difference=std::abs(actual.nd_corners[side][corner][axis]-expected.nd_corners[side][corner][axis]);
                max_difference=std::max(max_difference,difference);check(difference<1e-9,"ND click corner differs");
            }
        }
        const auto before=read(loaded);
        check(load(generated,model),error);
        check(before==read(loaded),"Existing ID profile overwritten");
        // Distinct fixture values prove which file supplied the loaded profile.
        auto modified=good;const auto uv=modified.find("0.18650067");check(uv!=std::string::npos,"Reference UV missing");
        modified.replace(uv,std::string("0.18650067").size(),"0.18");
        const auto priority=directory/"priority";
        write(priority/"a20n.json",good);write(priority/(original_id+".json"),modified);
        check(load(priority,model),error);
        check(loaded.filename()==original_id+".json"&&profile::current.nd_uv[0][0][0]==.18,"ID must beat base profile");
        std::filesystem::remove(loaded);
        check(load(priority,model),error);
        check(loaded==priority/"a20n.json"&&profile::current.nd_uv[0][0][0]==expected.nd_uv[0][0][0],"Base fallback failed");
        check(!std::filesystem::exists(priority/(original_id+".json")),"Base fallback generated an ID profile");
        check(read(priority/"a20n.json")==good,"Base profile overwritten");
        check(load(priority,model.parent_path()/"missing.acf"),error);
        check(loaded==priority/"a20n.json","Missing source must use base");
        write(priority/(original_id+".json"),"{}");
        check(!load(priority,model),"Invalid ID silently fell back to base");
        check(loaded.empty()&&profile::current.nd_uv[0][0][0]==0,"Failed load retained stale state");
        check(read(priority/(original_id+".json"))=="{}","Invalid ID overwritten");
        std::filesystem::remove(priority/(original_id+".json"));write(priority/"a20n.json","{}");
        check(!load(priority,model),"Invalid base silently regenerated");
        check(!std::filesystem::exists(priority/(original_id+".json")),"Invalid base caused generation");

        const auto fixture=directory/"derived-toliss-fixtures";
        std::filesystem::create_directories(fixture/"objects");
        const auto original_acf=read(model);
        const std::regex object_path(R"(P _obja/0/_v10_att_file_stl ([^\r\n]+))");
        std::smatch match;check(std::regex_search(original_acf,match,object_path),"Source cockpit attachment missing");
        const auto original_obj=read((model.parent_path()/"objects"/std::filesystem::u8path(match[1].str())).lexically_normal());
        write(fixture/"Cockpit_INN.OBJ",original_obj);
        auto fixture_acf=original_acf;
        fixture_acf.replace(static_cast<std::size_t>(match.position(1)),static_cast<std::size_t>(match.length(1)),"../Cockpit_INN.OBJ");
        const auto active=fixture/"a320ceo.acf";
        write(active,fixture_acf);
        const auto first_id=identify(active);
        write(fixture/"A20N.acf","invalid sibling");write(fixture/"backup_A20N.acf","invalid sibling");
        check(load(fixture/"config",active),error);
        check(loaded.filename()==first_id+".json","Sibling ACF replaced active model");
        write(active,fixture_acf+"\n# ACF content changed\n");
        const auto acf_changed=identify(active);
        check(acf_changed!=first_id&&acf_changed.substr(acf_changed.size()-5)==first_id.substr(first_id.size()-5),"ACF MD5 component did not change independently");
        check(load(fixture/"config",active),error);
        check(loaded.filename()==acf_changed+".json","ACF change reused old ID config");
        write(fixture/"Cockpit_INN.OBJ",original_obj+"\n# OBJ content changed\n");
        const auto obj_changed=identify(active);
        check(obj_changed.substr(0,obj_changed.size()-4)==acf_changed.substr(0,acf_changed.size()-4)&&obj_changed!=acf_changed,"OBJ MD5 component did not change independently");
        check(load(fixture/"config",active),error);
        check(loaded.filename()==obj_changed+".json","OBJ change reused old ID config");
        check(std::filesystem::exists(fixture/"config"/(first_id+".json"))&&std::filesystem::exists(fixture/"config"/(acf_changed+".json")),"Old ID configs not preserved");
        write(fixture/"renamed.acf",read(active));const auto renamed=identify(fixture/"renamed.acf");
        check(renamed=="renamed"+obj_changed.substr(std::string("a320ceo").size()),"Renaming changed hash components");

        for(const auto* code:{"A319","A321","A339"}) {
            auto text=fixture_acf;const auto at=text.find("P acf/_ICAO A20N");
            check(at!=std::string::npos,"Source ICAO marker missing");
            text.replace(at,std::string("P acf/_ICAO A20N").size(),std::string("P acf/_ICAO ")+code);
            const auto variant=fixture/(std::string(code)+".acf");write(variant,text);
            check(load(fixture/"config",variant,code),error);
            check(loaded.filename()==identify(variant)+".json","Derived model config not ID-named");
            check(!std::filesystem::exists(fixture/"config"/(std::string(code)+".json")),"Derived model generated base profile");
        }
        auto cockpit_only=fixture_acf;
        const auto at=cockpit_only.find("../Cockpit_INN.OBJ");
        cockpit_only.replace(at,std::string("../Cockpit_INN.OBJ").size(),"../unique_cockpit.obj");
        write(fixture/"unique_cockpit.obj",original_obj);write(active,cockpit_only);
        check(load(fixture/"cockpit-only-config",active),error);
        write(active,cockpit_only+"\nP _obja/999/_v10_att_file_stl ../other_cockpit.obj\n");
        check(!load(fixture/"ambiguous-config",active),"Ambiguous cockpit accepted");
        check(error.find("Ambiguous cockpit attachment")!=std::string::npos,"OBJ ambiguity reason missing");
        write(active,fixture_acf+"\nP _obja/999/_v10_att_file_stl ../other_cockpit.obj\n");
        check(load(fixture/"preferred-obj-config",active),error);
        write(active,fixture_acf+"\nP _obja/999/_v10_att_file_stl ../other_cockpit_INN.obj\n");
        check(!load(fixture/"ambiguous-inn-config",active),"Ambiguous cockpit+inn accepted");
        check(!load(generated,model,"../x"),"Invalid ICAO accepted");

        // Known MD5 vectors and raw CRLF bytes detect digest or text-mode mistakes.
        const auto hashes=directory/"hash-vectors";
        const std::string hash_acf="P _obja/0/_v10_att_file_stl ../cockpit_INN.obj\r\n";
        write(hashes/"vector.acf",hash_acf);write(hashes/"cockpit_INN.obj","");
        id=identify(hashes/"vector.acf");
        check(id.substr(id.size()-4)=="d41d","Empty-file MD5 vector failed");
        write(hashes/"cockpit_INN.obj","abc");
        const auto abc_id=identify(hashes/"vector.acf");
        check(abc_id.substr(abc_id.size()-4)=="9001","abc MD5 vector failed");
        check(abc_id.substr(0,abc_id.size()-4)==id.substr(0,id.size()-4),"OBJ changed ACF hash");
        write(hashes/"vector.acf","P _obja/0/_v10_att_file_stl ../cockpit_INN.obj\n");
        check(identify(hashes/"vector.acf")!=abc_id,"Raw CRLF/LF ACF bytes hashed identically");
        std::cout<<"PASS active ACF/attached OBJ identity, independent MD5 changes, raw-byte/known MD5 vectors\n";
        std::cout<<"PASS ID > distributed ICAO > ID generation; invalid profiles fail, no base generation/overwrite\n";
        std::cout<<"PASS cockpit filename preference/ambiguity, derived ICAOs (not real-aircraft validation)\n";
        std::cout<<"PASS A20N reference extraction; max corner difference="<<max_difference<<" m; ID="<<original_id<<'\n';
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
