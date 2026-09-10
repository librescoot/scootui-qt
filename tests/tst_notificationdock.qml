import QtQuick
import QtTest
import "../qml/notifications"
import "../qml/widgets/components"

TestCase {
    name: "UnifiedAttentionDock"
    when: windowShown
    visible: true
    width: 480
    height: 300

    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontBody: 18
        property int fontHero: 64
        property int radiusCard: 8
    }
    QtObject { id: service; property var presentation: ({}) }
    UnifiedAttentionDock { id: dock; width: 480; service: service }

    function init() {
        dock.isDark = true
        service.presentation = ({})
    }

    function show(main, companion, criticalCount) {
        service.presentation = ({main: main, companion: companion || {}, criticalCount: criticalCount || 0})
        waitForRendering(dock)
    }

    function fits(item, container) {
        verify(item !== null)
        const top = item.mapToItem(container, 0, 0)
        const bottom = item.mapToItem(container, item.width, item.height)
        verify(top.x >= 0 && top.y >= 0, item.objectName + " top " + top)
        verify(bottom.x <= container.width + 1 && bottom.y <= container.height + 1,
               item.objectName + " bottom " + bottom + " container height " + container.height)
    }

    function test_idle() {
        verify(!dock.visible)
        compare(dock.height, 0)
    }

    function test_notifications_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }

    function test_notifications(data) {
        dock.isDark = data.dark
        for (const priority of [0, 2, 3]) {
            show({id: "battery", kind: "warning", priority: priority,
                  title: "Battery 0: Multiple Critical Issues", body: "B6, B1: Stop safely and check battery"},
                 {kind: "nav", status: 2, distance: 120, maneuverType: 5,
                  instruction: "Turn right onto Main Street", street: "Main Street"}, 2)
            const card = findChild(dock, "attentionMainCard")
            const title = findChild(dock, "notificationTitle")
            verify(!title.truncated)
            fits(title, card)
            fits(findChild(dock, "notificationBody"), card)
            const companion = findChild(dock, "maneuverInstruction")
            compare(companion.text, "Turn right onto Main Street")
            verify(!companion.truncated)
            fits(companion, findChild(dock, "attentionCompanion"))
            verify(dock.height < 220)
        }
    }

    function test_rotatingNotificationsKeepCompanionAndStyle() {
        const nav = {kind: "nav", status: 2, distance: 80, maneuverType: 5,
                     instruction: "Turn right onto Main Street", compactInstruction: "Turn right"}
        for (const dark of [true, false]) {
            dock.isDark = dark
            show({id: "a", kind: "critical", priority: 0, title: "Battery fault", body: "Stop safely"}, nav, 2)
            const card = findChild(dock, "attentionMainCard")
            const color = card.color.toString()
            const titleSize = findChild(dock, "notificationTitle").font.pixelSize
            for (const title of ["Connection lost", "Battery fault"]) {
                show({id: title, kind: "critical", priority: 0, title: title, body: "Stop safely"}, nav, 2)
                compare(findChild(dock, "notificationTitle").text, title)
                compare(findChild(dock, "notificationTitle").font.pixelSize, titleSize)
                compare(findChild(dock, "attentionMainCard").color.toString(), color)
                compare(findChild(findChild(dock, "attentionCompanion"), "maneuverInstruction").text, "Turn right")
                fits(findChild(dock, "attentionCompanion"), dock)
            }
        }
    }

    function test_notificationTextIsPlain() {
        show({kind: "info", priority: 4, title: "<b>Plain title with enough words to wrap over multiple lines literally</b>",
              body: "<img src='https://example.invalid/image.png'>"})
        const title = findChild(dock, "notificationTitle")
        const body = findChild(dock, "notificationBody")
        compare(title.textFormat, Text.PlainText)
        compare(body.textFormat, Text.PlainText)
        compare(title.text, "<b>Plain title with enough words to wrap over multiple lines literally</b>")
        compare(findChild(title, "balancedTextProbe").textFormat, Text.PlainText)
        verify(title.lineCount > 1)
        fits(title, findChild(dock, "attentionMainCard"))
    }

    function test_navigationPayload_data() {
        return [
            {tag: "distant roundabout", distance: 650, type: 17},
            {tag: "roundabout", distance: 80, type: 17},
            {tag: "execute roundabout", distance: 0, type: 17},
            {tag: "left", distance: 50, type: 4},
            {tag: "right U-turn", distance: 50, type: 11}
        ]
    }

    function test_navigationPayload(data) {
        const maneuver = {kind: "nav", status: 2, distance: data.distance,
                          maneuverType: data.type, roundaboutExit: 2, street: "Ernst-Reuter-Platz",
                          instruction: "Take the second exit onto Ernst-Reuter-Platz",
                          nextStreet: "Straße des 17. Juni", nextType: 18, showNextPreview: true,
                          remainingDuration: 3600, distanceToDestination: 8500, eta: "12:30"}
        for (const dark of [true, false]) {
            dock.isDark = dark
            show(maneuver)
            const widget = findChild(dock, "attentionTurnByTurn")
            verify(widget !== null)
            verify(findChild(dock, "attentionMainCard") === null)
            compare(widget.maneuver, maneuver)
            const iconBox = findChild(dock, "maneuverIconBox")
            compare(iconBox.mType, data.type)
            compare(iconBox.isRoundabout, data.type === 17 && data.distance <= 500)
            if (iconBox.isRoundabout)
                compare(findChild(dock, "mainRoundaboutIcon").fallbackExitNumber, 2)
            const instruction = findChild(dock, "maneuverInstruction")
            compare(instruction.text, maneuver.instruction)
            verify(!instruction.truncated)
            fits(instruction, widget)
            fits(findChild(dock, "maneuverDistance"), widget)
            fits(findChild(dock, "maneuverNextPreview"), widget)
            const distance = findChild(widget, "maneuverDistance")
            const style = {instructionColor: instruction.color.toString(),
                           instructionSize: instruction.font.pixelSize,
                           distanceColor: distance.color.toString(),
                           distanceSize: distance.font.pixelSize, distanceText: distance.text}
            show({kind: "critical", priority: 0, title: "Motor short-circuit"}, maneuver)
            const compact = findChild(dock, "attentionCompanion")
            verify(compact.compact)
            const icon = findChild(compact, "maneuverIconBox")
            compare(icon.mType, data.type)
            compare(icon.isRoundabout, data.type === 17 && data.distance <= 500)
            if (icon.isRoundabout) compare(findChild(compact, "mainRoundaboutIcon").fallbackExitNumber, 2)
            const compactInstruction = findChild(compact, "maneuverInstruction")
            const compactDistance = findChild(compact, "maneuverDistance")
            compare(compactInstruction.color.toString(), style.instructionColor)
            compare(compactInstruction.font.pixelSize, style.instructionSize)
            compare(compactDistance.color.toString(), style.distanceColor)
            compare(compactDistance.font.pixelSize, style.distanceSize)
            compare(compactDistance.text, style.distanceText)
            verify(!findChild(compact, "maneuverTripSummary").visible)
            verify(!findChild(compact, "maneuverNextPreview").visible)
            fits(compactInstruction, compact)
        }
    }

    function test_longInstructionAndStart() {
        const instruction = "Turn right onto the very long street name leading towards the city centre and continue past the railway station"
        for (const start of [true, false]) {
            show({kind: "nav", status: 2, isStart: start, distance: 0, maneuverType: 5,
                  instruction: instruction, street: "Street only", showNextPreview: true,
                  nextType: 20, remainingDuration: 7200, distanceToDestination: 18000, eta: "15:30"})
            const widget = findChild(dock, "attentionTurnByTurn")
            const text = findChild(dock, "maneuverInstruction")
            compare(text.text, instruction)
            verify(text.lineCount <= 3)
            fits(text, widget)
            const summary = findChild(dock, "maneuverTripSummary")
            verify(text.mapToItem(widget, 0, 0).y >= summary.height)
            fits(summary, widget)
            fits(findChild(dock, "maneuverNextPreview"), widget)
            compare(findChild(dock, "maneuverDistance").visible, !start)
            verify(dock.height < 220)
        }
    }

    function test_compactTextAndBaseline() {
        for (const dark of [true, false]) {
            dock.isDark = dark
            for (const shortText of ["Turn right", ""]) {
                const nav = {kind: "nav", status: 2, distance: 80, maneuverType: 5,
                             instruction: "Turn right onto Main Street", compactInstruction: shortText}
                show(nav)
                compare(findChild(dock, "maneuverInstruction").text, nav.instruction)
                show({kind: "critical", priority: 0, title: "Stop safely"}, nav)
                const widget = findChild(dock, "attentionCompanion")
                const instruction = findChild(widget, "maneuverInstruction")
                const distance = findChild(widget, "maneuverDistance")
                compare(instruction.text, shortText || nav.instruction)
                fuzzyCompare(instruction.mapToItem(widget, 0, instruction.baselineOffset).y,
                             distance.mapToItem(widget, 0, distance.baselineOffset).y, 0.1)
                fits(instruction, widget)
                fits(distance, widget)
            }
        }
    }

    function test_longCompanionIsBounded() {
        show({kind: "critical", priority: 0,
              title: "Battery 0: Multiple Critical Issues", body: "Stop safely and check the battery"},
             {kind: "nav", status: 2, distance: 80, maneuverType: 17, roundaboutExit: 3,
              instruction: "Take the third exit onto the very long road name towards the central railway station and continue straight ahead"})
        const companion = findChild(dock, "attentionCompanion")
        const text = findChild(companion, "maneuverInstruction")
        verify(text.text.indexOf("Take the third exit") === 0)
        verify(text.lineCount <= 2)
        fits(text, companion)
        fits(findChild(companion, "maneuverIconBox"), companion)
        fits(companion, dock)
        verify(dock.height < 220)
    }

    function test_notificationStylePreservesNavigationTypography_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }

    function test_notificationStylePreservesNavigationTypography(data) {
        dock.isDark = data.dark
        const nav = {kind: "nav", status: 2, maneuverType: 5, distance: 80,
                     instruction: "Turn right onto Main Street", compactInstruction: "Turn right"}
        show(nav)
        const background = findChild(dock, "maneuverBackground").color.toString()
        const instructionSize = findChild(dock, "maneuverInstruction").font.pixelSize
        for (const severity of [{kind: "error", priority: 0}, {kind: "critical", priority: 0},
                                {kind: "warning", priority: 2}, {kind: "success", priority: 3},
                                {kind: "info", priority: 4}, {kind: "debug", priority: 5}]) {
            const entry = {kind: severity.kind, priority: severity.priority, title: "Vehicle status updated",
                           body: "Check the details before continuing"}
            for (const mode of ["full", "compact", "secondary"]) {
                show(mode === "secondary" ? nav : entry, mode === "full" ? {} : mode === "secondary" ? entry : nav)
                const card = findChild(dock, mode === "secondary" ? "attentionCompanionNotification" : "attentionMainCard")
                verify(card.color.toString() !== background)
                if (severity.kind === "error" || severity.kind === "critical")
                    verify(card.color.r > card.color.g && card.color.r > card.color.b)
                else if (severity.kind === "warning")
                    verify(card.color.r > card.color.g && card.color.g > card.color.b)
                else if (severity.kind === "success")
                    verify(card.color.g > card.color.r && card.color.g > card.color.b)
                else if (severity.kind === "info")
                    verify(card.color.b > card.color.r && card.color.b > card.color.g)
                else
                    verify(Math.abs(card.color.r - card.color.b) < 0.05)
                fuzzyCompare(card.color.a, 0.8, 0.01)
                compare(card.border.width, 0)
                for (const child of card.children)
                    verify(!(child.width === 3 && child.height === card.height), "No left accent stripe")
                const title = findChild(card, "notificationTitle")
                compare(title.font.pixelSize, instructionSize + 2)
                compare(title.font.weight, Font.DemiBold)
                compare(findChild(card, "notificationBody").font.pixelSize, instructionSize)
                compare(findChild(card, "notificationIcon").color, card.accent)
                if (severity.kind === "info")
                    compare(findChild(card, "notificationIcon").text, MaterialIcon.iconInfoOutline)
                else if (severity.kind === "debug")
                    compare(findChild(card, "notificationIcon").text, MaterialIcon.iconBugReport)
                compare(title.maximumLineCount, mode === "secondary" ? 1 : 2)
                verify(!title.truncated)
                fits(title, card)
                if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
                    grabImage(dock).save(captureDirectory + "/notification-style-" + severity.kind + "-" + mode + "-" + data.tag + ".png")
            }
        }
    }

    function test_pairedPrimaryKeepsNormalManeuver_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }

    function test_pairedPrimaryKeepsNormalManeuver(data) {
        dock.isDark = data.dark
        for (const kind of ["success", "info"]) {
            for (const longText of [false, true]) {
                const nav = {kind: "nav", status: 2, maneuverType: 5, distance: 80,
                             instruction: longText ? "Turn right onto the very long street name towards the railway station and continue ahead"
                                                   : "Turn right onto Main Street",
                             compactInstruction: "Turn right", showNextPreview: true, nextStreet: "Station"}
                show(nav, {kind: kind, priority: kind === "success" ? 3 : 4,
                           title: longText ? "Download complete with additional details about the downloaded maps" : "Download complete",
                           body: longText ? "The selected map regions are now available for offline use" : ""})
                const widget = findChild(dock, "attentionTurnByTurn")
                const secondary = findChild(dock, "attentionCompanionNotification")
                verify(widget.paired && !widget.compact)
                compare(widget.height, 96)
                compare(findChild(widget, "maneuverIconBox").width, 80)
                compare(findChild(widget, "maneuverIconBox").height, 80)
                compare(findChild(widget, "maneuverGlyph").font.pixelSize, themeStore.fontHero)
                const instruction = findChild(widget, "maneuverInstruction")
                compare(instruction.text, nav.instruction)
                compare(instruction.maximumLineCount, 2)
                verify(instruction.width >= 380)
                if (!longText) verify(!instruction.truncated)
                fits(instruction, widget)
                fits(findChild(widget, "maneuverIconBox"), widget)
                fits(findChild(widget, "maneuverDistance"), widget)
                verify(!findChild(widget, "maneuverTripSummary").visible)
                verify(!findChild(widget, "maneuverNextPreview").visible)
                verify(secondary.visible)
                compare(findChild(secondary, "notificationTitle").maximumLineCount, 1)
                compare(findChild(secondary, "notificationBody").visible, longText)
                fits(secondary, dock)
                verify(dock.height <= 152)
            }
        }
    }

    function test_queuedCountsWrapWithoutTakingContentWidth_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }

    function test_queuedCountsWrapWithoutTakingContentWidth(data) {
        dock.isDark = data.dark
        for (const values of [
                 {error: 12, warning: 23, success: 34, info: 45, debug: 56},
                 {error: 100, warning: 1234, success: 99999, info: 2147483647, debug: 56},
                 {error: 2147483647, warning: 2147483647, success: 2147483647, info: 2147483647, debug: 2147483647}]) {
            service.presentation = {main: {kind: "critical", priority: 0,
                title: "Battery 0: Multiple Critical Issues", body: "Stop safely and check the battery"},
                companion: {kind: "nav", status: 2, maneuverType: 5, distance: 80, instruction: "Turn right"},
                queuedCounts: values}
            waitForRendering(dock)
            const card = findChild(dock, "attentionMainCard")
            const counts = findChild(card, "queuedNotificationCounts")
            const title = findChild(card, "notificationTitle")
            const body = findChild(card, "notificationBody")
            compare(counts.width, 112)
            verify(counts.height > 20 && counts.height <= 100)
            verify(title.maxWidth >= 296)
            verify(!title.truncated)
            fits(title, card)
            fits(body, card)
            fits(counts, card)
            verify(title.mapToItem(card, title.width, 0).x < counts.x)
            verify(body.mapToItem(card, body.width, 0).x < counts.x)
            for (const kind of counts.visibleKinds) {
                const badge = findChild(counts, "queuedCount_" + kind)
                compare(badge.count, values[kind])
                compare(badge.text, "+" + values[kind])
                compare(badge.lineCount, 1)
                verify(!badge.truncated)
                fits(badge, counts)
            }
            verify(dock.height <= 156)
        }
    }

    function test_loadingClearsRetainedDirectionalIcons_data() {
        const cases = []
        for (const type of [2, 3, 17, 18]) {
            for (const paired of [true, false]) {
                for (const dark of [true, false])
                    cases.push({tag: type + (paired ? "-paired" : "-full") + (dark ? "-dark" : "-light"),
                                type: type, paired: paired, dark: dark})
            }
        }
        return cases
    }

    function test_loadingClearsRetainedDirectionalIcons(data) {
        dock.isDark = data.dark
        const nav = {kind: "nav", status: 2, maneuverType: data.type, distance: 80,
                     instruction: "Keep the current direction", roundaboutExit: 2}
        const companion = data.paired ? {kind: "info", priority: 4, title: "Trip updated"} : {}
        show(nav, companion)
        const widget = findChild(dock, "attentionTurnByTurn")
        const icon = findChild(widget, "maneuverIconBox")
        verify(icon.isRoundabout || icon.isKeepFork)
        for (const status of [1, 3]) {
            show(Object.assign({}, nav, {status: status}), companion)
            compare(widget.compact, false)
            compare(widget.paired, data.paired)
            compare(icon.mType, data.type)
            compare(icon.mDist, 80)
            verify(!icon.isRoundabout && !icon.isKeepFork)
            verify(findChild(widget, "mainRoundaboutIcon") === null)
            verify(!findChild(widget, "maneuverForkIcon").visible)
            const glyph = findChild(widget, "maneuverGlyph")
            verify(glyph.visible)
            compare(glyph.text, MaterialIcon.iconNavigation)
            verify(findChild(widget, "navigationLoadingIndicator").running)
            if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0) {
                wait(200)
                grabImage(dock).save(captureDirectory + "/navigation-retained-loading-" + status + "-" + data.tag + ".png")
            }
        }
        show(nav, companion)
        verify(icon.isRoundabout || icon.isKeepFork)
        verify(!findChild(widget, "navigationLoadingIndicator").running)
    }

    function test_navigationStatus_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }

    function test_navigationStatus(data) {
        dock.isDark = data.dark
        for (const status of [1, 3]) {
            show({kind: "nav", status: status})
            const label = findChild(dock, "maneuverInstruction")
            verify(label.visible)
            verify(label.text.length > 0)
            fits(label, dock)
            const indicator = findChild(dock, "navigationLoadingIndicator")
            verify(indicator.running)
            compare(indicator.palette.text.toString(), data.dark ? "#ffffff" : "#212121")
            compare(indicator.palette.dark.toString(), indicator.palette.text.toString())
            verify(!findChild(dock, "maneuverDistance").visible)
            verify(!findChild(dock, "maneuverTripSummary").visible)
            if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
                grabImage(dock).save(captureDirectory + "/navigation-loading-" + status + "-" + data.tag + ".png")
        }
    }
}
