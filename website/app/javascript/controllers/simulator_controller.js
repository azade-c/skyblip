import { Controller } from "@hotwired/stimulus"

const MAX_CATCHUP_MS = 250
const ACTIVATION_KEYS = [" ", "Enter"]

export default class extends Controller {
  static targets = ["canvas", "status", "start"]
  static values = { src: String, on: String, off: String, settings: String }

  disconnect() {
    this.#stop()
    this.sim = null
    this.element.classList.remove("simulator--running", "simulator--off")
    this.startTarget.disabled = false
  }

  async start() {
    this.startTarget.disabled = true
    const { load, PAGES } = await import(this.srcValue)
    this.pages = PAGES
    this.sim = await load()
    this.element.classList.add("simulator--running")
    this.#run()
  }

  restart() {
    this.#stop()
    this.sim = null
    this.element.classList.remove("simulator--off")
    this.startTarget.disabled = false
    this.start()
  }

  hold(event) {
    if (!this.#accepts(event)) return
    event.preventDefault()
    this.sim.holdButton(1)
  }

  release() {
    if (this.sim) this.sim.holdButton(0)
  }

  touch(event) {
    if (!this.#accepts(event)) return
    event.preventDefault()
    this.sim.holdPad(1)
  }

  lift() {
    if (this.sim) this.sim.holdPad(0)
  }

  #accepts(event) {
    if (!this.sim) return false
    if (event.type !== "keydown") return true
    return !event.repeat && ACTIVATION_KEYS.includes(event.key)
  }

  #run() {
    this.lastMs = performance.now()
    const frame = () => {
      this.#advance()
      this.#paint()
      this.timer = requestAnimationFrame(frame)
    }
    this.timer = requestAnimationFrame(frame)
  }

  #advance() {
    const now = performance.now()
    const elapsed = Math.min(now - this.lastMs, MAX_CATCHUP_MS)
    this.lastMs = now
    this.sim.advance(this.sim.elapsedMs() + elapsed)
  }

  #paint() {
    this.sim.paint(this.canvasTarget)
    const powered = this.sim.powered() === 1
    this.element.classList.toggle("simulator--off", !powered)
    this.statusTarget.textContent = `${this.#screen()} · ${powered ? this.onValue : this.offValue}`
  }

  #screen() {
    return this.sim.settingsOpen() === 1 ? this.settingsValue : this.pages[this.sim.page()]
  }

  #stop() {
    if (this.timer) cancelAnimationFrame(this.timer)
    this.timer = null
  }
}
