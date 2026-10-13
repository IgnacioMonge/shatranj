#include "pc/client/stockfish_release.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

static const char kRelease[] = R"({
  "tag_name": "sf_19",
  "assets": [
    {"name": "stockfish-linux-arm64-universal.tar.gz", "size": 80181250,
     "digest": null,
     "browser_download_url": "https://example.test/linux-arm64.tar.gz"},
    {"name": "stockfish-linux-x86-64-universal.tar.gz", "size": 81388977,
     "digest": "sha256:9DEFC0D4E55D49C65A6D042F3E571A39FCEA499ADE6DBE741B53B8C65E03611F",
     "browser_download_url": "https://example.test/linux-x86-64.tar.gz"},
    {"name": "stockfish-windows-x86-64-universal.zip", "size": 81431614,
     "digest": "sha256:3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7",
     "browser_download_url": "https://example.test/windows-x86-64.zip"},
    {"name": "stockfish-macos-universal.tar.gz", "size": 82323876,
     "digest": "sha256:a1f0e3bcc5a6927a11fe6fc8e54a779754645f3c2bae2cf13420fd1957adaa77",
     "browser_download_url": "http://example.test/macos.tar.gz"}
  ]
})";

int main()
{
    using namespace StockfishRelease;

    check(platformPrefixes("windows", "x86_64", "x86_64") ==
              QStringList{"stockfish-windows-x86-64"},
          "windows x64 prefix");
    check(platformPrefixes("windows", "arm64", "x86_64") ==
              (QStringList{"stockfish-windows-arm64", "stockfish-windows-x86-64"}),
          "native CPU first, client build CPU as fallback");
    check(platformPrefixes("macos", "arm64", "arm64") ==
              QStringList{"stockfish-macos-universal"},
          "macOS universal build");

    StockfishAsset asset;
    check(pickAsset(kRelease, platformPrefixes("windows", "x86_64", "x86_64"), &asset) &&
              asset.tag == "sf_19" &&
              asset.name == "stockfish-windows-x86-64-universal.zip" &&
              asset.size == 81431614 &&
              asset.sha256 == "3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7",
          "windows asset");
    check(pickAsset(kRelease, platformPrefixes("linux", "arm64", "x86_64"), &asset) &&
              asset.name == "stockfish-linux-x86-64-universal.tar.gz" &&
              asset.sha256.startsWith("9defc0"),
          "asset without digest is skipped; digest is lowercased");
    check(!pickAsset(kRelease, platformPrefixes("macos", "arm64", "arm64"), &asset),
          "non-HTTPS download is rejected");
    check(!pickAsset(kRelease, {"stockfish-linux-riscv64"}, &asset),
          "missing platform");
    check(!pickAsset("not json", {"stockfish-windows-x86-64"}, &asset),
          "malformed release JSON");

    check(pickPinned(platformPrefixes("windows", "x86_64", "x86_64"), &asset) &&
              asset.tag == "sf_19" &&
              asset.name == "stockfish-windows-x86-64-universal.zip" &&
              asset.url == QUrl("https://github.com/official-stockfish/Stockfish/releases/"
                                "download/sf_19/stockfish-windows-x86-64-universal.zip") &&
              asset.size == 81431614 &&
              asset.sha256 == "3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7",
          "pinned windows asset when the API refuses the query");
    check(pickPinned(platformPrefixes("macos", "arm64", "arm64"), &asset) &&
              asset.name == "stockfish-macos-universal.tar.gz" &&
              asset.sha256.startsWith("a1f0e3"),
          "pinned macOS asset");
    check(pickPinned(platformPrefixes("linux", "riscv64", "x86_64"), &asset) &&
              asset.name == "stockfish-linux-riscv64-universal.tar.gz",
          "pinned native linux asset first");
    check(!pickPinned({"stockfish-haiku-x86-64"}, &asset), "pinned missing platform");

    QTemporaryDir dir;
    QDir(dir.path()).mkpath("stockfish/wiki");
    const auto writeFile = [&](const QString &name, int size) {
        QFile file(dir.path() + "/" + name);
        check(file.open(QIODevice::WriteOnly) && file.write(QByteArray(size, 'x')) == size,
              "write fixture");
    };
    writeFile("stockfish/Copying.txt", 100);
    writeFile("stockfish/wiki/Home.md", 50);
    writeFile("stockfish/stockfish-windows-x86-64-universal.exe", 1000);
    writeFile("stockfish/stockfish.txt", 10);
    check(findBinary(dir.path()).endsWith("stockfish/stockfish-windows-x86-64-universal.exe"),
          "largest stockfish file is the engine");
    check(findBinary(dir.path() + "/missing").isEmpty(), "missing directory");

    if (failures != 0) {
        return 1;
    }
    std::printf("stockfish release tests ok\n");
    return 0;
}
