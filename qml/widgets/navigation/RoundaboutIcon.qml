import QtQuick

Canvas {
    id: root

    property int exitNumber: 1
    property bool isDark: true
    property real size: 48

    width: size
    height: size

    onExitNumberChanged: requestPaint()
    onIsDarkChanged: requestPaint()
    onSizeChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();

        var cx = size / 2;
        var cy = size / 2;
        var ringRadius = size * 0.22;
        var ringWidth = size * 0.08;
        var stubLength = size * 0.12;
        var entryStubLength = size * 0.22;
        var exitStubLength = size * 0.25;
        var stubWidth = size * 0.1;

        var activeColor = isDark ? "rgba(255,255,255,1.0)" : "rgba(51,51,51,1.0)";
        var subduedColor = isDark ? "rgba(255,255,255,0.3)" : "rgba(0,0,0,0.3)";

        // Entry is always at bottom (pi/2 in canvas coords)
        var entryAngle = Math.PI / 2;

        // Calculate exit geometry
        var exitGeometry = calculateExitGeometry(exitNumber, entryAngle);
        var exitAngle = exitGeometry.exitAngle;
        var sweepAngle = exitGeometry.sweepAngle;

        // 1. Draw subdued roundabout ring
        ctx.beginPath();
        ctx.arc(cx, cy, ringRadius, 0, 2 * Math.PI);
        ctx.strokeStyle = subduedColor;
        ctx.lineWidth = ringWidth;
        ctx.stroke();

        // 2. Draw subdued exit stubs at 90-degree intervals
        for (var i = 1; i <= 4; i++) {
            var angle = entryAngle - (i * Math.PI / 2);
            drawStub(ctx, cx, cy, angle, ringRadius, stubLength, stubWidth, subduedColor);
        }

        // 3. Draw active entry stub at bottom
        drawStub(ctx, cx, cy, entryAngle, ringRadius, entryStubLength, stubWidth, activeColor);

        // 4. Draw active route arc from entry to exit
        ctx.beginPath();
        // Canvas arc goes clockwise by default; sweepAngle is negative (counter-clockwise)
        // arc(x, y, r, startAngle, endAngle, anticlockwise)
        if (sweepAngle < 0) {
            ctx.arc(cx, cy, ringRadius, entryAngle, entryAngle + sweepAngle, true);
        } else {
            ctx.arc(cx, cy, ringRadius, entryAngle, entryAngle + sweepAngle, false);
        }
        ctx.strokeStyle = activeColor;
        ctx.lineWidth = ringWidth;
        ctx.lineCap = "round";
        ctx.stroke();

        // 5. Draw active exit stub with arrow
        drawStubWithArrow(ctx, cx, cy, exitAngle, ringRadius, exitStubLength, stubWidth, activeColor);

        // 6. Reset lineCap for clean state
        ctx.lineCap = "butt";
    }

    function calculateExitGeometry(exit, entryAngle) {
        // Each exit is approximately 90 degrees apart
        // Counter-clockwise in canvas coords (negative sweep)
        var sweep = -exit * (Math.PI / 2);

        var exitAng = (entryAngle + sweep) % (2 * Math.PI);
        if (exitAng < 0) {
            exitAng += 2 * Math.PI;
        }

        return { exitAngle: exitAng, sweepAngle: sweep };
    }

    function drawStub(ctx, cx, cy, angle, ringRadius, stubLen, stubW, color) {
        var startDist = ringRadius;
        var endDist = ringRadius + stubLen;

        var sx = cx + startDist * Math.cos(angle);
        var sy = cy + startDist * Math.sin(angle);
        var ex = cx + endDist * Math.cos(angle);
        var ey = cy + endDist * Math.sin(angle);

        var perpAngle = angle + Math.PI / 2;
        var hw = stubW / 2;

        var p1x = sx + hw * Math.cos(perpAngle);
        var p1y = sy + hw * Math.sin(perpAngle);
        var p2x = sx - hw * Math.cos(perpAngle);
        var p2y = sy - hw * Math.sin(perpAngle);
        var p3x = ex - hw * Math.cos(perpAngle);
        var p3y = ey - hw * Math.sin(perpAngle);
        var p4x = ex + hw * Math.cos(perpAngle);
        var p4y = ey + hw * Math.sin(perpAngle);

        ctx.beginPath();
        ctx.moveTo(p1x, p1y);
        ctx.lineTo(p4x, p4y);
        ctx.lineTo(p3x, p3y);
        ctx.lineTo(p2x, p2y);
        ctx.closePath();
        ctx.fillStyle = color;
        ctx.fill();
    }

    function drawStubWithArrow(ctx, cx, cy, angle, ringRadius, stubLen, stubW, color) {
        var startDist = ringRadius;
        var arrowStartDist = ringRadius + stubLen * 0.5;
        var arrowTipDist = ringRadius + stubLen;

        var sx = cx + startDist * Math.cos(angle);
        var sy = cy + startDist * Math.sin(angle);
        var asx = cx + arrowStartDist * Math.cos(angle);
        var asy = cy + arrowStartDist * Math.sin(angle);
        var atx = cx + arrowTipDist * Math.cos(angle);
        var aty = cy + arrowTipDist * Math.sin(angle);

        var perpAngle = angle + Math.PI / 2;
        var halfStubW = stubW / 2;
        var halfArrowW = stubW * 1.3;

        // Stub body corners
        var sl1x = sx + halfStubW * Math.cos(perpAngle);
        var sl1y = sy + halfStubW * Math.sin(perpAngle);
        var sl2x = asx + halfStubW * Math.cos(perpAngle);
        var sl2y = asy + halfStubW * Math.sin(perpAngle);

        var sr1x = sx - halfStubW * Math.cos(perpAngle);
        var sr1y = sy - halfStubW * Math.sin(perpAngle);
        var sr2x = asx - halfStubW * Math.cos(perpAngle);
        var sr2y = asy - halfStubW * Math.sin(perpAngle);

        // Arrow wing points
        var alx = asx + halfArrowW * Math.cos(perpAngle);
        var aly = asy + halfArrowW * Math.sin(perpAngle);
        var arx = asx - halfArrowW * Math.cos(perpAngle);
        var ary = asy - halfArrowW * Math.sin(perpAngle);

        ctx.beginPath();
        ctx.moveTo(sl1x, sl1y);
        ctx.lineTo(sl2x, sl2y);
        ctx.lineTo(alx, aly);
        ctx.lineTo(atx, aty);
        ctx.lineTo(arx, ary);
        ctx.lineTo(sr2x, sr2y);
        ctx.lineTo(sr1x, sr1y);
        ctx.closePath();
        ctx.fillStyle = color;
        ctx.fill();
    }
}
