import QtQuick
import QtTest
import "../qml/screens/MapDownloadPolicy.js" as Policy

TestCase {
    name: "MapDownloadPolicy"

    // A region restored from cached metadata is enough to download; only an
    // unknown region needs the fix that would resolve it.
    function test_knownRegionNeedsNoFix() {
        verify(Policy.regionResolved("Niedersachsen (incl. Bremen)"))
        verify(!Policy.gpsRequired("Niedersachsen (incl. Bremen)", true))
        verify(Policy.canDownload(0, 0, true, false, "Niedersachsen (incl. Bremen)", true))
    }

    function test_unknownRegionNeedsFixForDownload() {
        verify(!Policy.regionResolved(""))
        verify(Policy.gpsRequired("", true))
        verify(!Policy.canDownload(0, 0, true, false, "", true))
        verify(Policy.canDownload(0, 0, true, true, "", true))
    }

    // Nothing to fetch means no fix is worth asking for, known region or not.
    function test_nothingToDownloadNeedsNoFix() {
        verify(!Policy.gpsRequired("", false))
        verify(!Policy.canDownload(0, 0, true, false, "", false))
    }

    function test_offlineOrBusyCannotDownload() {
        verify(!Policy.canDownload(0, 0, false, true, "berlin_brandenburg", true))
        verify(!Policy.canDownload(1, 0, true, true, "berlin_brandenburg", true))
    }

    function test_componentFetchedWhenMissingOrStale() {
        verify(Policy.willDownload(true, false, false))
        verify(Policy.willDownload(true, true, true))
        verify(!Policy.willDownload(true, true, false))
        verify(!Policy.willDownload(false, false, true))
    }

    // The estimate must cover the side being fetched, not both regardless.
    function test_sizeCoversOnlyFetchedSides() {
        const display = 100 * 1048576
        const routing = 300 * 1048576
        compare(Policy.sizeLabel("Niedersachsen (incl. Bremen)", display, routing, true, false),
                "Niedersachsen (incl. Bremen) (100 MB)")
        compare(Policy.sizeLabel("Niedersachsen (incl. Bremen)", display, routing, false, true),
                "Niedersachsen (incl. Bremen) (300 MB)")
        compare(Policy.sizeLabel("Niedersachsen (incl. Bremen)", display, routing, true, true),
                "Niedersachsen (incl. Bremen) (400 MB)")
    }

    function test_sizeFallsBackToRegionWhenUnknown() {
        compare(Policy.sizeLabel("Bayern", 0, 0, true, true), "Bayern")
    }
}
