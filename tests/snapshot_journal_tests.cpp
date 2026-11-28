#include "nexa/snapshot_journal.hpp"
#include "test_process.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static void write(const std::filesystem::path& p, const std::string& value) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << value;
}
static std::string read(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "nexa-snapshot-journal-test";
    const auto target = root / "disk.img";
    const auto backup = root / "disk.rollback";
    const auto journal = root / "transaction.journal";
    fs::create_directories(root);
    if (argc == 3 && std::string(argv[1]) == "--crash-child") {
        write(target, "original-disk-state");
        auto entries = nexa::snapshot::prepare(journal, {{target, backup}});
        write(target, argv[2]);
        nexa::snapshot::syncFile(target);
        std::_Exit(73); // Simulate abrupt process death; no destructors or rollback.
    }

    // Abrupt termination after only the first disk has been replaced must
    // still roll back the complete multi-disk transaction in a fresh process.
    if (argc == 3 && std::string(argv[1]) == "--publication-crash-child") {
        const fs::path journal=argv[2];
        const auto temp=fs::path(journal.string()+".tmp");
        write(temp,"NEXA_SNAPSHOT_JOURNAL 1\\nPREPARED\\n999\\ntruncated");
        nexa::snapshot::syncFile(temp);
        std::_Exit(86); // Dies before atomic rename publishes the journal.
    }
    if (argc == 4 && std::string(argv[1]) == "--group-crash-child") {
        const fs::path a=argv[2], b=argv[3];
        const auto groupRoot=a.parent_path();
        const auto groupJournal=groupRoot/"crash-group.journal";
        const auto aBackup=groupRoot/"crash-a.rollback";
        const auto bBackup=groupRoot/"crash-b.rollback";
        const auto aLink=fs::path(a.string()+".nexa-journal");
        const auto bLink=fs::path(b.string()+".nexa-journal");
        nexa::snapshot::prepareGroup(groupJournal,{{aLink,{a,aBackup}},{bLink,{b,bBackup}}});
        write(a,"disk-a-replaced-before-crash");
        nexa::snapshot::syncFile(a);
        std::_Exit(85);
    }

    for (const std::string phase : {"after-disk-replacement", "after-vm-load"}) {
        write(target, "pre-crash-placeholder");
        const int status = nexa_test::runChildProcess(argv[0], {"--crash-child", phase});
        if (status == 0) { std::cerr << "child unexpectedly exited normally\n"; return 1; }
        if (!fs::exists(journal)) { std::cerr << "journal missing after crash\n"; return 1; }
        nexa::snapshot::recover(journal);
        if (read(target) != "original-disk-state") {
            std::cerr << "rollback did not restore original bytes\n"; return 1;
        }
        if (fs::exists(journal) || fs::exists(backup)) {
            std::cerr << "recovery left transaction debris\n"; return 1;
        }
    }
    write(target, "committed-before-crash");
    auto entries = nexa::snapshot::prepare(journal, {{target, backup}});
    write(target, "new-committed-state");
    nexa::snapshot::syncFile(target);
    nexa::snapshot::writeJournal(journal, nexa::snapshot::JournalPhase::Committed, entries);
    nexa::snapshot::recover(journal);
    if (read(target) != "new-committed-state") {
        std::cerr << "committed transaction was incorrectly rolled back\n"; return 1;
    }
    // A single coordinator decision must recover every disk, even when
    // recovery is first triggered by attaching only the second disk.
    const auto diskA = root / "disk-a.img";
    const auto diskB = root / "disk-b.img";
    const auto backupA = root / "disk-a.rollback";
    const auto backupB = root / "disk-b.rollback";
    const auto sidecarA = fs::path(diskA.string() + ".nexa-journal");
    const auto sidecarB = fs::path(diskB.string() + ".nexa-journal");
    const auto coordinator = root / "coordinated.journal";
    write(diskA, "disk-a-original");
    write(diskB, "disk-b-original");
    nexa::snapshot::prepareGroup(coordinator, {
        {sidecarA, {diskA, backupA}},
        {sidecarB, {diskB, backupB}}
    });
    write(diskA, "disk-a-mutated");
    write(diskB, "disk-b-mutated");
    nexa::snapshot::syncFile(diskA);
    nexa::snapshot::syncFile(diskB);
    nexa::snapshot::recover(sidecarB);
    if (read(diskA) != "disk-a-original" || read(diskB) != "disk-b-original" ||
        fs::exists(sidecarA) || fs::exists(sidecarB) || fs::exists(coordinator) ||
        fs::exists(backupA) || fs::exists(backupB)) {
        std::cerr << "coordinated rollback did not restore and clean every disk\\n";
        return 1;
    }

    write(diskA, "disk-a-before-commit");
    write(diskB, "disk-b-before-commit");
    nexa::snapshot::prepareGroup(coordinator, {
        {sidecarA, {diskA, backupA}},
        {sidecarB, {diskB, backupB}}
    });
    write(diskA, "disk-a-committed");
    write(diskB, "disk-b-committed");
    nexa::snapshot::syncFile(diskA);
    nexa::snapshot::syncFile(diskB);
    nexa::snapshot::commitGroup(coordinator);
    // A crash during rollback-directory cleanup leaves the COMMITTED decision
    // for the next process to consume without rolling either disk back.
    nexa::snapshot::recoverCoordinator(coordinator);
    if (read(diskA) != "disk-a-committed" || read(diskB) != "disk-b-committed" ||
        fs::exists(sidecarA) || fs::exists(sidecarB) || fs::exists(coordinator) ||
        fs::exists(backupA) || fs::exists(backupB)) {
        std::cerr << "coordinated commit did not retain and clean every disk\\n";
        return 1;
    }


    // Windows uses MoveFileExW(REPLACE_EXISTING); exercise replacing an
    // already-published PREPARED journal with its durable COMMITTED decision.
    const auto replacementTarget = root / "replacement.img";
    const auto replacementBackup = root / "replacement.rollback";
    const auto replacementJournal = root / "replacement.journal";
    write(replacementTarget, "replacement-before");
    auto replacementEntries = nexa::snapshot::prepare(
        replacementJournal, {{replacementTarget, replacementBackup}});
    write(replacementTarget, "replacement-after");
    nexa::snapshot::syncFile(replacementTarget);
    nexa::snapshot::writeJournal(replacementJournal,
        nexa::snapshot::JournalPhase::Committed, replacementEntries);
    nexa::snapshot::JournalPhase replacementPhase{};
    (void)nexa::snapshot::readJournal(replacementJournal, replacementPhase);
    if (replacementPhase != nexa::snapshot::JournalPhase::Committed ||
        fs::exists(fs::path(replacementJournal.string() + ".tmp"))) {
        std::cerr << "atomic journal replacement did not publish COMMITTED phase\\n";
        return 1;
    }
    nexa::snapshot::recover(replacementJournal);
    if (read(replacementTarget) != "replacement-after") {
        std::cerr << "journal replacement rolled back committed target\\n";
        return 1;
    }

    // Simulate termination after the first rollback backup has already been
    // deleted: COMMITTED cleanup must not need any remaining rollback image.
    write(diskA, "cleanup-a-before");
    write(diskB, "cleanup-b-before");
    nexa::snapshot::prepareGroup(coordinator, {
        {sidecarA, {diskA, backupA}}, {sidecarB, {diskB, backupB}}
    });
    write(diskA, "cleanup-a-committed");
    write(diskB, "cleanup-b-committed");
    nexa::snapshot::syncFile(diskA);
    nexa::snapshot::syncFile(diskB);
    nexa::snapshot::writeJournal(coordinator,
        nexa::snapshot::JournalPhase::Committed,
        nexa::snapshot::readJournal(coordinator, replacementPhase));
    fs::remove(backupA); // Crash point: partial backup cleanup has happened.
    nexa::snapshot::recoverCoordinator(coordinator);
    if (read(diskA) != "cleanup-a-committed" ||
        read(diskB) != "cleanup-b-committed" ||
        fs::exists(coordinator) || fs::exists(backupB) ||
        fs::exists(sidecarA) || fs::exists(sidecarB)) {
        std::cerr << "partial committed cleanup did not recover cleanly\\n";
        return 1;
    }

    // Journal publication uses a temp file and atomic rename. A process that
    // dies while writing the temp file must leave the old target untouched.
    const auto publicationTarget=root/"publication-target.img";
    const auto publicationJournal=root/"publication.journal";
    const auto publicationTemp=fs::path(publicationJournal.string()+".tmp");
    write(publicationTarget,"publication-original");
    const int publicationStatus = nexa_test::runChildProcess(
        argv[0], {"--publication-crash-child", publicationJournal.string()});
const int expectedPublicationStatus = 86;
    if (publicationStatus!=expectedPublicationStatus ||
        read(publicationTarget)!="publication-original" || fs::exists(publicationJournal) ||
        !fs::exists(publicationTemp)) {
        std::cerr<<"partial journal publication did not remain isolated in temp file\\n";return 1;
    }
    fs::remove(publicationTemp);

    // Corrupt one backup in a coordinated transaction: validation must fail
    // before either live disk is overwritten.
    write(diskA,"disk-a-corruption-original");
    write(diskB,"disk-b-corruption-original");
    nexa::snapshot::prepareGroup(coordinator,{{sidecarA,{diskA,backupA}},{sidecarB,{diskB,backupB}}});
    write(diskA,"disk-a-corruption-mutated");
    write(diskB,"disk-b-corruption-mutated");
    write(backupB,"tampered-backup");
    bool corruptionRejected=false;
    try { nexa::snapshot::recover(sidecarA); }
    catch (const std::runtime_error&) { corruptionRejected=true; }
    if (!corruptionRejected || read(diskA)!="disk-a-corruption-mutated" ||
        read(diskB)!="disk-b-corruption-mutated") {
        std::cerr<<"corrupt backup was not rejected atomically\\n";return 1;
    }
    // Repair the fixture to permit deterministic cleanup.
    write(backupB,"disk-b-corruption-original");
    nexa::snapshot::recover(sidecarA);

    // Exercise a real child process crash halfway through coordinated restore.
    write(diskA,"disk-a-process-original");
    write(diskB,"disk-b-process-original");
    const int groupStatus = nexa_test::runChildProcess(
        argv[0], {"--group-crash-child", diskA.string(), diskB.string()});
const int expectedGroupStatus = 85;
    if(groupStatus!=expectedGroupStatus) {
        std::cerr<<"multi-disk child did not terminate at intended fault point\\n";return 1;
    }
    nexa::snapshot::recover(sidecarB);
    if(read(diskA)!="disk-a-process-original"||read(diskB)!="disk-b-process-original") {
        std::cerr<<"multi-disk restart recovery was not atomic\\n";return 1;
    }

    fs::remove_all(root);
    std::cout << "Snapshot journal subprocess recovery tests passed\n";
    return 0;
}
