/*
 * Copyright (c) 2020 Ableton AG, Berlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

class MeterView: UIView {
  var levelInDb = -Double.infinity {
    didSet {
      update()
    }
  }
  private var ledLayers : [CAShapeLayer] = []

  private static let ledSize = CGSize(width: 5, height: 5)
  private static let minPaddingBetweenLeds : CGFloat = 3.0
  private static let ledInactiveColor = UIColor.systemGray4.cgColor
  private static let ledActiveColor = UIColor.systemBlue.cgColor
  private static let meterRangeDb = (-35.0 ... 0.0)

  override init(frame: CGRect) {
    super.init(frame: frame)
    setupLeds()
  }

  required init?(coder: NSCoder) {
    super.init(coder: coder)
    setupLeds()
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    setupLeds()
  }

  private func setupLeds() {
    let numLeds = Int(
      (bounds.size.width - MeterView.ledSize.width)
        / (MeterView.ledSize.width + MeterView.minPaddingBetweenLeds)) + 1
    let whitespace = bounds.size.width - (CGFloat(numLeds) * MeterView.ledSize.width)
    let paddingBetweenLeds = whitespace / CGFloat(numLeds - 1)

    ledLayers.removeAll()
    for i in 0..<numLeds {
      let layer = CAShapeLayer()
      layer.anchorPoint = CGPoint(x: 0.0, y: 0.0)
      layer.frame = CGRect(
        origin: CGPoint(
          x: (MeterView.ledSize.width + paddingBetweenLeds) * CGFloat(i),
          y: bounds.size.height / 2.0 - MeterView.ledSize.height / 2.0),
        size: MeterView.ledSize)
      layer.path = CGPath(ellipseIn: layer.bounds, transform: nil)
      layer.fillColor = MeterView.ledInactiveColor
      ledLayers.append(layer)
    }

    layer.sublayers = ledLayers
  }

  private static func normalized(levelInDb: Double) -> Double {
    let unclampedValue = (levelInDb - MeterView.meterRangeDb.lowerBound)
        / (MeterView.meterRangeDb.upperBound - MeterView.meterRangeDb.lowerBound)
    return (0.0 ... 1.0).clamp(value: unclampedValue)
  }

  private func update() {
    let numActiveLeds = Int(
      (MeterView.normalized(levelInDb: levelInDb) * Double(ledLayers.count)).rounded())
    for (ledIndex, ledLayer) in ledLayers.enumerated() {
      ledLayer.fillColor =
        ledIndex < numActiveLeds ? MeterView.ledActiveColor : MeterView.ledInactiveColor
    }
  }
}
