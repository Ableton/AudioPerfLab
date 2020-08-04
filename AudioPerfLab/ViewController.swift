/*
 * Copyright (c) 2019 Ableton AG, Berlin
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

import UIKit
import os

class ViewController: UITableViewController {
  private var engine = Engine()
  private var displayLink: CADisplayLink?
  private var coreActivityViews: [ActivityView] = []
  private var tableViewHeaders: [CollapsibleTableViewHeader] = []
  private var lastNumFrames: Int32?
  private var waitingToChangeInput = false
  private var inputMeterSmoother = MeterSmoother()

  private var lastEnergyUsageTime: Double?
  private var lastEnergyUsage: Double?
  private var lastPowerLabelUpdateTime: Double?
  private var lastEnergyUsageForPowerLabel: Double?

  @IBOutlet weak private var freezeActivityViewsSwitch: FreezeSwitch!
  @IBOutlet weak private var driveDurationsView: ActivityView!
  @IBOutlet weak private var workDistributionView: UIView!
  @IBOutlet weak private var workDistributionOneThreadWarning: UILabel!
  @IBOutlet weak private var coreActivityStackView: UIStackView!
  @IBOutlet weak private var energyUsageView: ActivityView!

  @IBOutlet weak private var inputMeterView: MeterView!
  @IBOutlet weak private var isAudioInputEnabledSwitch: UISwitch!
  @IBOutlet weak private var bufferSizeStepper: UIStepper!
  @IBOutlet weak private var bufferSizeField: UITextField!
  @IBOutlet weak private var numSinesSlider: SliderWithValue!
  @IBOutlet weak private var numBurstSinesSlider: SliderWithValue!

  @IBOutlet weak private var numBusyThreadsSlider: SliderWithValue!
  @IBOutlet weak private var busyThreadPeriodSlider: SliderWithValue!
  @IBOutlet weak private var busyThreadCpuUsageSlider: SliderWithValue!

  @IBOutlet weak private var numProcessingThreadsSlider: SliderWithValue!
  @IBOutlet weak private var minimumLoadSlider: SliderWithValue!
  @IBOutlet weak private var processInDriverThreadControl: UISegmentedControl!
  @IBOutlet weak private var isWorkIntervalOnSwitch: UISwitch!

  private static let maxEnergyViewPowerInWatts = 5.0
  private static let powerLabelUpdateInterval = 0.5

  private static let activityViewDuration = 3.0
  private static let activityViewLatency = 0.1
  private static let activityViewExtraBufferingDuration = activityViewLatency * 2
  private static let dropoutColor = UIColor.red
  private static let threadColors = [
    UIColor.black,
    UIColor.blue,
    UIColor.green,
    UIColor.magenta,
    UIColor.orange,
    UIColor.purple,
    UIColor.red,
    UIColor.yellow,
  ]

  override func viewDidLoad() {
    super.viewDidLoad()

    for section in 0..<tableView.numberOfSections {
      let title = tableView(tableView, titleForHeaderInSection: section)!
      tableViewHeaders.append(makeTableViewHeader(title: title))
    }
    tableViewHeader("Work Distribution")!.isExpanded = false
    tableViewHeader("Cores")!.isExpanded = false
    tableViewHeader("Energy")!.isExpanded = false

    displayLink = CADisplayLink(target: self, selector: #selector(displayLinkStep))
    displayLink!.add(to: .main, forMode: RunLoop.Mode.common)
    setupDriveDurationsView()
    setupWorkDistributionViews()
    setupCoreActivityViews()
    setupEnergyUsageView()

    let percentageFormatter = { (value: Float) in return "\(Int(value * 100))%"}
    minimumLoadSlider.valueFormatter = percentageFormatter
    busyThreadCpuUsageSlider.valueFormatter = percentageFormatter

    busyThreadPeriodSlider.valueFormatter =
      { (value: Float) in return "\(Int(value * 1000))ms"}

    initalizeControls()
  }

  private func setupDriveDurationsView() {
    driveDurationsView.duration = ViewController.activityViewDuration
    driveDurationsView.extraBufferingDuration =
      ViewController.activityViewExtraBufferingDuration
  }

  private func setupWorkDistributionViews() {
    for _ in 0..<MAX_NUM_THREADS {
      let workActivityView = ActivityView(frame: workDistributionView.bounds)
      workActivityView.duration = ViewController.activityViewDuration
      workActivityView.extraBufferingDuration =
        ViewController.activityViewExtraBufferingDuration
      workActivityView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      workActivityView.contentMode = .scaleAspectFit
      workDistributionView.addSubview(workActivityView)
    }
  }

  private func setupCoreActivityViews() {
    for i in 0..<numberOfProcessors {
      let coreActivityView = ActivityView(frame: .zero)
      coreActivityView.duration = ViewController.activityViewDuration
      coreActivityView.extraBufferingDuration =
        ViewController.activityViewExtraBufferingDuration
      coreActivityView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      coreActivityView.contentMode = .scaleAspectFit
      coreActivityViews.append(coreActivityView)

      let label = UILabel(frame: CGRect(x: 0, y: 0, width: 9, height: 0))
      label.textAlignment = .center
      label.font = UIFont.systemFont(ofSize: 9)
      label.text = String(i + 1)
      label.autoresizingMask = [.flexibleHeight]
      label.backgroundColor = UIColor.white.withAlphaComponent(0.5)

      let rowForCore = UIView(frame: .zero)
      rowForCore.addSubview(coreActivityView)
      rowForCore.addSubview(label)
      coreActivityStackView.addArrangedSubview(rowForCore)
    }
  }

  private func setupEnergyUsageView() {
    energyUsageView.duration = ViewController.activityViewDuration
    energyUsageView.extraBufferingDuration =
      ViewController.activityViewExtraBufferingDuration
  }

  private func initalizeControls() {
    isAudioInputEnabledSwitch.isOn = engine.isAudioInputEnabled
    bufferSizeStepper.value = log2(Double(engine.preferredBufferSize))
    bufferSizeField.text = String(engine.preferredBufferSize)
    numSinesSlider.value = Float(engine.numSines)
    numSinesSlider.minimumValue = Float(engine.numSines)
    numSinesSlider.maximumValue = Float(engine.maxNumSines)
    numBurstSinesSlider.maximumValue = Float(engine.maxNumSines)
    numProcessingThreadsSlider.value = Float(engine.numProcessingThreads)
    minimumLoadSlider.value = Float(engine.minimumLoad)
    numBusyThreadsSlider.value = Float(engine.numBusyThreads)
    busyThreadPeriodSlider.value = Float(engine.busyThreadPeriod)
    busyThreadCpuUsageSlider.value = Float(engine.busyThreadCpuUsage)
    processInDriverThreadControl.selectedSegmentIndex =
      engine.processInDriverThread ? 1 : 0
    isWorkIntervalOnSwitch.isOn = engine.isWorkIntervalOn
    updateThreadDependentControls()
  }

  private func updateThreadDependentControls() {
    isWorkIntervalOnSwitch.isEnabled = engine.numWorkerThreads > 0
    workDistributionOneThreadWarning.isHidden = engine.numProcessingThreads > 1
  }

  private func updateNumEngineWorkerThreads() {
    engine.numWorkerThreads =
      Int32(numProcessingThreadsSlider.value) - (engine.processInDriverThread ? 1 : 0)
    updateThreadDependentControls()
  }

  @IBAction func activityViewsFrozenChanged(_ sender: Any) {
    let isFrozen = freezeActivityViewsSwitch.isOn
    activityViews().forEach { $0.isFrozen = isFrozen }
    redrawExpandedActivityViews()
  }

  @IBAction private func isAudioInputEnabledChanged(_ sender: Any) {
    if waitingToChangeInput {
      return
    }

    // Fade out and wait some time before toggling the input to avoid an audible glitch
    // due to a CoreAudio bug.
    let fadeDuration = 0.01
    let glitchAvoidanceDelay = 0.4

    engine.setOutputVolume(0.0, fadeDuration: fadeDuration)
    waitingToChangeInput = true

    DispatchQueue.main.asyncAfter(deadline: .now() + glitchAvoidanceDelay) { [weak self] in
      guard let self = self else {
        return
      }

      self.engine.isAudioInputEnabled = self.isAudioInputEnabledSwitch.isOn
      self.engine.setOutputVolume(1.0, fadeDuration: fadeDuration)
      self.waitingToChangeInput = false
    }
  }

  @IBAction private func bufferSizeChanged(_ sender: Any) {
    engine.preferredBufferSize = 1 << Int(bufferSizeStepper.value)
    bufferSizeField.text = String(engine.preferredBufferSize)
  }

  @IBAction private func numSinesChanged(_ sender: Any) {
    engine.numSines = Int32(numSinesSlider.value)
  }

  @IBAction private func numProcessingThreadsChanged(_ sender: Any) {
    updateNumEngineWorkerThreads()
  }

  @IBAction private func minimumLoadChanged(_ sender: Any) {
    engine.minimumLoad = Double(minimumLoadSlider.value)
  }

  @IBAction private func numBusyThreadsChanged(_ sender: Any) {
    engine.numBusyThreads = Int32(numBusyThreadsSlider.value)
  }

  @IBAction private func busyThreadPeriodChanged(_ sender: Any) {
    engine.busyThreadPeriod = Double(busyThreadPeriodSlider.value)
  }

  @IBAction private func busyThreadCpuUsageChanged(_ sender: Any) {
    engine.busyThreadCpuUsage = Double(busyThreadCpuUsageSlider.value)
  }

  @IBAction private func processInDriverThreadChanged(_ sender: Any) {
    engine.processInDriverThread = processInDriverThreadControl.selectedSegmentIndex == 1
    updateNumEngineWorkerThreads()
  }

  @IBAction private func isWorkIntervalOnChanged(_ sender: Any) {
    engine.isWorkIntervalOn = isWorkIntervalOnSwitch.isOn
  }

  @IBAction private func playSineBurst(_ sender: Any) {
    engine.playSineBurst(for: 0.25, additionalSines: Int32(numBurstSinesSlider.value))
  }

  func makeTableViewHeader(title: String) -> CollapsibleTableViewHeader {
    let headerView = CollapsibleTableViewHeader(frame: .zero)
    headerView.title = title
    headerView.onTap = {
      self.tableView.beginUpdates()
      self.tableView.endUpdates()
      self.redrawExpandedActivityViews()
    }
    return headerView
  }

  func tableViewHeader(_ title: String) -> CollapsibleTableViewHeader? {
    return tableViewHeaders.first(where: { $0.title == title })
  }

  override func tableView(
    _ tableView: UITableView,
    heightForHeaderInSection section: Int) -> CGFloat {
    return 40.0
  }

  override func tableView(
    _ tableView: UITableView,
    heightForRowAt indexPath: IndexPath) -> CGFloat {
    if tableViewHeaders[indexPath.section].isExpanded {
      return super.tableView(tableView, heightForRowAt: indexPath)
    } else {
      // Keep a separator line for collapsed sections
      return indexPath.row == 0 ? 0.5 : 0.0
    }
  }

  override func tableView(
    _ tableView: UITableView,
    viewForHeaderInSection section: Int) -> UIView? {
    return tableViewHeaders[section]
  }

  static private func getActivePartialsProcessed(
    from measurement: DriveMeasurement) -> [Int] {
    return Mirror(reflecting: measurement.numActivePartialsProcessed).children.map {
      Int($0.value as! Int32)
    }
  }

  static private func getCpuNumbers(from measurement: DriveMeasurement) -> [Int] {
    return Mirror(reflecting: measurement.cpuNumbers).children.map {
      Int($0.value as! Int32)
    }
  }

  static private func getThreadIndexPerCpu(from measurement: DriveMeasurement) -> [Int?] {
    var threadIndexPerCpu = [Int?](repeating: nil, count: numberOfProcessors)
    for (threadIndex, cpuNum) in getCpuNumbers(from: measurement).enumerated() {
      if cpuNum >= 0 {
        threadIndexPerCpu[cpuNum] = threadIndex
      }
    }
    return threadIndexPerCpu
  }

  private func addLoadMeasurement(
    time: Double,
    duration bufferDuration: Double,
    measurement: DriveMeasurement) {
    let color =
      measurement.duration <= bufferDuration ? UIColor.black : ViewController.dropoutColor
    driveDurationsView.addSample(
      time: time,
      duration: bufferDuration,
      value: measurement.duration / bufferDuration,
      color: color)
  }

  private func addWorkDistributionMeasurement(
    time: Double,
    duration bufferDuration: Double,
    measurement: DriveMeasurement) {
    let activePartialsProcessed =
      ViewController.getActivePartialsProcessed(from: measurement)
    let totalNumActivePartialsProcessed = activePartialsProcessed.reduce(
      0, { $0 + max(0, $1) })

    // Create a stacked area chart by overlaying ActivityViews. Start with the top-most
    // view (on the z-axis) to draw the bottom-most area for the first thread.
    var lastValue = 0.0
    for (threadIndex, workActivityView) in
      self.workDistributionView.subviews.reversed().enumerated() {
      let workActivityView = workActivityView as! ActivityView
      let partialsProcessed = activePartialsProcessed[threadIndex]

      if partialsProcessed >= 0 {
        let percent =
          Double(partialsProcessed) / Double(totalNumActivePartialsProcessed)
        let value = percent + lastValue
        let color = ViewController.threadColors[threadIndex]
        workActivityView.addSample(
          time: time,
          duration: bufferDuration,
          value: value,
          color: color)
        lastValue = value
      }
    }
  }

  private func addCoreMeasurement(
    time: Double,
    duration bufferDuration: Double,
    measurement: DriveMeasurement) {
    let threadIndexPerCpu = ViewController.getThreadIndexPerCpu(from: measurement)
    for (cpuNumber, coreActivityView) in coreActivityViews.enumerated() {
      let threadIndex = threadIndexPerCpu[cpuNumber]
      let color =
        threadIndex != nil ? ViewController.threadColors[threadIndex!] : UIColor.white
      coreActivityView.addSample(
        time: time,
        duration: bufferDuration,
        value: threadIndex != nil ? 1.0 : 0.0,
        color: color)
    }
  }

  private func fetchDriveMeasurements() {
    engine.fetchMeasurements({ measurement in
      if measurement.numFrames != self.lastNumFrames {
        os_log("Actual Buffer Size: %d", measurement.numFrames)
        self.lastNumFrames = measurement.numFrames
      }

      let duration = Double(measurement.numFrames) / self.engine.sampleRate
      let time = measurement.hostTime - duration
      self.addLoadMeasurement(time: time, duration: duration, measurement: measurement)
      self.addWorkDistributionMeasurement(
        time: time, duration: duration, measurement: measurement)
      self.addCoreMeasurement(time: time, duration: duration, measurement: measurement)
      self.inputMeterSmoother.addPeak(ampToDb(Double(measurement.inputPeakLevel)))
    })
  }

  private func fetchPowerMeasurements() {
    let energyHeader = tableViewHeader("Energy")!
    guard let energyUsage = taskEnergyUsage else {
      energyHeader.value = "Not Available"
      return
    }

    let time = CACurrentMediaTime()

    if let lastEnergyUsageTime = lastEnergyUsageTime,
       let lastEnergyUsage = lastEnergyUsage {
      let timeDelta = time - lastEnergyUsageTime
      let powerInWatts = (energyUsage - lastEnergyUsage) / timeDelta
      let normalizedPower = powerInWatts / ViewController.maxEnergyViewPowerInWatts
      energyUsageView.addSample(
        time: lastEnergyUsageTime,
        duration: timeDelta,
        value: normalizedPower,
        color: UIColor.black)
    }

    if (lastPowerLabelUpdateTime == nil
      || (time - lastPowerLabelUpdateTime!) > ViewController.powerLabelUpdateInterval) {
      if let lastPowerLabelUpdateTime = lastPowerLabelUpdateTime,
         let lastEnergyUsageForPowerLabel = lastEnergyUsageForPowerLabel {
        let timeDelta = time - lastPowerLabelUpdateTime
        let powerInWatts = (energyUsage - lastEnergyUsageForPowerLabel) / timeDelta
        energyHeader.value = String(format: "%.2f Watts", powerInWatts)
      }

      lastPowerLabelUpdateTime = time
      lastEnergyUsageForPowerLabel = energyUsage
    }

    lastEnergyUsageTime = time
    lastEnergyUsage = energyUsage
  }

  private func activityViews() -> [ActivityView] {
    return [driveDurationsView, energyUsageView]
      + workDistributionView.subviews.compactMap { $0 as? ActivityView }
      + coreActivityViews
  }

  private func redrawExpandedActivityViews() {
    if tableViewHeader("Load")!.isExpanded {
      driveDurationsView.setNeedsDisplay()
    }
    if tableViewHeader("Work Distribution")!.isExpanded {
      workDistributionView.subviews
        .compactMap { $0 as? ActivityView }
        .forEach { $0.setNeedsDisplay() }
    }
    if tableViewHeader("Cores")!.isExpanded {
      coreActivityViews.forEach { $0.setNeedsDisplay() }
    }
    if tableViewHeader("Energy")!.isExpanded {
      energyUsageView.setNeedsDisplay()
    }
  }

  @objc private func displayLinkStep(displayLink: CADisplayLink) {
    fetchDriveMeasurements()
    fetchPowerMeasurements()

    let startTime = displayLink.timestamp -
      ViewController.activityViewDuration - ViewController.activityViewLatency
    activityViews().forEach { $0.startTime = startTime }
    if !freezeActivityViewsSwitch.isOn {
      inputMeterView.levelInDb = inputMeterSmoother.smoothedLevel(
        displayTime: displayLink.timestamp)
      redrawExpandedActivityViews()
    }
  }
}

extension Engine {
  var numProcessingThreads: Int {
    get {
      return Int(numWorkerThreads + (processInDriverThread ? 1 : 0))
    }
  }
}

fileprivate var numberOfProcessors: Int {
  return ProcessInfo.processInfo.processorCount
}

// Energy usage of the current task in joules
fileprivate var taskEnergyUsage: Double? {
#if arch(arm) || arch(arm64)
  let TASK_POWER_INFO_V2_COUNT =
    MemoryLayout<task_power_info_v2>.stride/MemoryLayout<natural_t>.stride
  var info = task_power_info_v2()
  var count = mach_msg_type_number_t(TASK_POWER_INFO_V2_COUNT)

  let kerr = withUnsafeMutablePointer(to: &info) {
    $0.withMemoryRebound(to: integer_t.self, capacity: TASK_POWER_INFO_V2_COUNT) {
        task_info(mach_task_self_,
                  task_flavor_t(TASK_POWER_INFO_V2),
                  $0,
                  &count)
    }
  }

  return kerr == KERN_SUCCESS ? Double(info.task_energy) * 1.0e-9 : nil
#else
  return nil
#endif
}
