// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

import UIKit

class ActivityView: UIView {
  struct Point {
    let value : Double
    let color : UIColor
  }

  var duration = 0.0 {
    didSet {
      initializePointsArray()
    }
  }
  var extraBufferingDuration = 0.0 {
    didSet {
      initializePointsArray()
    }
  }
  var startTime = 0.0

  private var points: [Point] = []
  private var endTime: Double?
  private var lastWritePosition: Double?

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    initialize()
  }

  override init(frame: CGRect) {
    super.init(frame: frame)
    initialize()
  }

  private func initialize() {
    isOpaque = false
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    initializePointsArray()
  }

  private func initializePointsArray() {
    let numExtraBufferingPoints = pointsPerSecond() * extraBufferingDuration
    let numPoints = Int(Double(frame.size.width) + numExtraBufferingPoints)
    if points.count != numPoints {
      points = Array(repeating: Point(value: 0.0, color: UIColor.black), count: numPoints)
      endTime = nil
      lastWritePosition = nil
    }
  }

  func addSample(
    time: Double,
    duration sampleDuration: Double,
    value: Double,
    color: UIColor) {
    if let endTime = endTime {
      let missingTimeInPoints = timeToPosition(time - endTime)
      if missingTimeInPoints >= 0.1 {
        addPoints(position: timeToPosition(endTime),
                    length: missingTimeInPoints,
                     value: 0.0,
                     color: UIColor.clear)
      }
    }

    let startPosition = timeToPosition(time)
    let durationInPoints = timeToPosition(sampleDuration)
    addPoints(position: startPosition,
                length: durationInPoints,
                 value: value,
                 color: color)
    
    endTime = time + sampleDuration
  }

  override func draw(_ rect: CGRect) {
    guard let endTime = endTime, !points.isEmpty else { return }

    let path = UIBezierPath()
    path.move(to: CGPoint(x: 0.0, y: self.frame.height))

    let startPosition = timeToPosition(startTime)
    let endPosition = timeToPosition(min(startTime + duration, endTime))
    let drawWidth = max(0.0, endPosition - startPosition)
    let readIndex =
      Int(startPosition.truncatingRemainder(dividingBy: Double(points.count)))
    var currentColor = points[readIndex].color
    for x in 0..<Int(drawWidth) {
      let dataIndex = (readIndex + x) % points.count
      let sample = points[dataIndex]
      let y = CGFloat(1.0 - sample.value) * self.frame.height

      if sample.color != currentColor {
        let previousX = path.currentPoint.x

        path.addLine(to: CGPoint(x: previousX, y: self.frame.height))
        path.close()
        currentColor.setFill()
        path.fill()
        path.removeAllPoints()

        currentColor = sample.color
        path.move(to: CGPoint(x: previousX, y: self.frame.height))
        path.addLine(to: CGPoint(x: previousX, y: y))
      }

      path.addLine(to: CGPoint(x: CGFloat(x), y: y))
    }
    path.addLine(to: CGPoint(x: CGFloat(drawWidth), y: path.currentPoint.y))
    path.addLine(to: CGPoint(x: CGFloat(drawWidth), y: self.frame.height))
    path.close()
    currentColor.setFill()
    path.fill()
  }

  private func addPoints(
    position: Double,
    length: Double,
    value: Double,
    color: UIColor) {
    addPoint(position: position, value: value, color: color)

    let pinnedLength = min(length, Double(points.count))
    for p in stride(from: floor(position) + 1.0, to: position + pinnedLength, by: 1.0) {
      addPoint(position: p, value: value, color: color)
    }
  }

  private func addPoint(position: Double, value: Double, color: UIColor) {
    guard !points.isEmpty else { return }

    let i = Int(position.truncatingRemainder(dividingBy: Double(points.count)))
    let newPeak = lastWritePosition == nil || floor(lastWritePosition!) != floor(position)
      ? value : max(points[i].value, value)
    points[i] = Point(value: newPeak, color: color)
    lastWritePosition = position
  }

  private func pointsPerSecond() -> Double {
    return duration == 0.0 ? 0.0 : Double(self.frame.width) / duration
  }

  private func timeToPosition(_ time: Double) -> Double {
    return time * pointsPerSecond()
  }
}
