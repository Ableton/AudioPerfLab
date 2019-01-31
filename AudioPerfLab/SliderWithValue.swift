// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

import UIKit

@IBDesignable
class SliderWithValue: UISlider {
  let kValueFieldWidth: CGFloat = 53.0
  let kPadding: CGFloat = 16.0
  var valueFormatter = { (value: Float) in return "\(Int(value))" }
  private var valueField = UITextField()

  override var value: Float {
    didSet {
      updateValueField()
    }
  }

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    initialize()
  }

  override init(frame: CGRect) {
    super.init(frame: frame)
    initialize()
  }

  private func initialize() {
    valueField.borderStyle = .roundedRect
    valueField.font = UIFont.systemFont(ofSize: 15.0)
    valueField.textAlignment = .center
    valueField.isUserInteractionEnabled = false
    valueField.translatesAutoresizingMaskIntoConstraints = false
    addSubview(valueField)
    updateValueField()

    let valueFieldWidthConstraint = NSLayoutConstraint(
      item: valueField, attribute: .width, relatedBy: .equal, toItem: nil,
      attribute: .notAnAttribute, multiplier: 1.0, constant: kValueFieldWidth)
    let valueFieldConstraint = NSLayoutConstraint(
      item: valueField, attribute: .trailing, relatedBy: .equal, toItem: self,
      attribute: .trailing, multiplier: 1.0, constant: 0)
    NSLayoutConstraint.activate([valueFieldWidthConstraint, valueFieldConstraint])

    self.addTarget(self, action: #selector(onValueChange), for: .valueChanged)
  }

  override func trackRect(forBounds bounds: CGRect) -> CGRect {
    var rect = super.trackRect(forBounds: bounds)
    rect.size.width = rect.width - kValueFieldWidth - kPadding
    return rect
  }

  @objc private func onValueChange() {
    updateValueField()
  }

  private func updateValueField() {
    valueField.text = valueFormatter(value)
  }
}
