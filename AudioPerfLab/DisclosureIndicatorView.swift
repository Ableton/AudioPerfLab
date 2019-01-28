// Copyright: 2019, Ableton AG, Berlin. All rights reserved.

import UIKit

class DisclosureIndicatorView: UIView {
  var isExpanded = true {
    didSet {
      UIView.animate(withDuration: 0.2) {
        self.updateRotation()
      }
    }
  }

  override open var intrinsicContentSize: CGSize {
    return shapeLayer.frame.size
  }

  private let edgeLength = 9.0
  private let lineWidth = 2.0
  private let color = UIColor(
    red: 199.0 / 255.0, green: 199.0 / 255.0, blue: 204.0 / 255.0, alpha: 1.0);
  private let shapeLayer = CAShapeLayer()

  override init(frame: CGRect) {
    super.init(frame: frame)

    let path = CGMutablePath()
    path.addRects([
      CGRect(x: 0, y: 0, width: edgeLength, height: lineWidth),
      CGRect(x: edgeLength - lineWidth, y: 0, width: lineWidth, height: edgeLength)])

    shapeLayer.path = path
    shapeLayer.fillColor = color.cgColor
    shapeLayer.frame = CGRect(x: 0, y: 0, width: edgeLength, height: edgeLength)
    updateRotation()
    layer.addSublayer(shapeLayer)
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    shapeLayer.position = CGPoint(x: bounds.midX, y: bounds.midY)
  }

  private func updateRotation() {
    shapeLayer.setAffineTransform(CGAffineTransform(
      rotationAngle: isExpanded ? (3.0 * .pi / 4.0) : (.pi / 4.0)))
  }
}
