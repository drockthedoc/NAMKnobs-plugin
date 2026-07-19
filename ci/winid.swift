// NAMKnobs CI helper: print the CGWindowID of the standalone app's largest on-screen window.
//
// Why: the GUI-screenshot step captures a live standalone instance so the dynamic per-pedal knob layout
// isn't a blind spot. Full-screen `screencapture -x` picks up whatever else is on screen -- notably the
// macOS screen-recording permission dialog, which pops after the first capture and covers the knob area.
// Capturing a specific window's layer (`screencapture -l<id>`) grabs only that window's composited image,
// so overlays owned by other processes (the permission dialog) are excluded. This helper finds that id.
//
// Prints the window number to stdout (or "0" if not found). Owner name is the process/executable name,
// which the workflow renames to "NAMKnobs" before launch.
import CoreGraphics
import Foundation

let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as? [[String: Any]] ?? []
var bestNum = 0
var bestArea = 0.0
for w in list
{
  guard let owner = w[kCGWindowOwnerName as String] as? String, owner == "NAMKnobs" else { continue }
  guard let num = w[kCGWindowNumber as String] as? Int else { continue }
  let b = w[kCGWindowBounds as String] as? [String: Double] ?? [:]
  let area = (b["Width"] ?? 0) * (b["Height"] ?? 0)
  if area > bestArea
  {
    bestArea = area
    bestNum = num
  }
}
print(bestNum)
