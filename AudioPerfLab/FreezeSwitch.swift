// Copyright: 2019, Ableton AG, Berlin. All rights reserved.

import UIKit

class FreezeSwitchView: UILabel {
  var isOn = false {
    didSet {
      textColor = isOn ? UIColor.white : tintColor
      shapeLayer.fillColor = isOn ? tintColor.cgColor : nil
    }
  }

  override open var intrinsicContentSize: CGSize {
    let padding: CGFloat = 5
    let contentSize = super.intrinsicContentSize
    return CGSize(
      width: contentSize.width + padding, height: contentSize.height + padding)
  }

  private var shapeLayer = CAShapeLayer()

  override init(frame: CGRect) {
    super.init(frame: frame)
    text = "❄\u{FE0E}"
    textAlignment = .center
    textColor = tintColor
    font = font.withSize(20.0)

    shapeLayer.lineWidth = 1.1
    shapeLayer.strokeColor = tintColor.cgColor
    shapeLayer.fillColor = nil
    layer.addSublayer(shapeLayer)
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    shapeLayer.frame = bounds
    shapeLayer.path = CGPath(
      ellipseIn: CGRect(
        x: 0, y: bounds.midY - bounds.size.width / 2,
        width: bounds.size.width, height: bounds.size.width),
      transform: nil)
  }
}

class FreezeSwitch: UIBarButtonItem {
  var isOn: Bool {
    get { return freezeSwitch.isOn }
    set { freezeSwitch.isOn = newValue }
  }

  private let freezeSwitch = FreezeSwitchView(frame: .zero)

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)

    let label = UILabel(frame: .zero)
    let attributes: [NSAttributedString.Key : Any] = [
      .font: UIFont.systemFont(ofSize: 13.0),
      .foregroundColor: label.tintColor]
    label.attributedText =
      NSAttributedString(string: "Freeze Visualizations", attributes: attributes)

    let stackView = UIStackView(arrangedSubviews: [freezeSwitch, label])
    stackView.spacing = 6
    stackView.alignment = .fill
    customView = stackView

    stackView.addGestureRecognizer(UITapGestureRecognizer(
      target:self, action: #selector(tap)))
  }

  @objc private func tap() {
    freezeSwitch.isOn = !freezeSwitch.isOn

    if let action = action {
      _ = target?.perform(action, with: self)
    }
  }
}
