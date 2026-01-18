import type {ReactNode} from 'react';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import useBaseUrl from '@docusaurus/useBaseUrl';
import {useEffect, useRef, useState} from 'react';
import styles from './index.module.css';

export default function Home(): ReactNode {
  const {siteConfig} = useDocusaurusContext();
  const [showChippy, setShowChippy] = useState(false);
  const hideTimerRef = useRef<number | null>(null);

  const cancelHide = () => {
    if (hideTimerRef.current !== null) {
      window.clearTimeout(hideTimerRef.current);
      hideTimerRef.current = null;
    }
  };

  const scheduleHide = () => {
    cancelHide();
    hideTimerRef.current = window.setTimeout(() => {
      setShowChippy(false);
    }, 3000);
  };

  useEffect(() => {
    return () => {
      cancelHide();
    };
  }, []);

  return (
    <Layout title={siteConfig.title} description={siteConfig.tagline}>
      <main className={styles.page}>
        <section className={styles.hero}>
          <div className={styles.heroInner}>
            <div className={styles.heroCopy}>
              <Heading as="h1" className={styles.title}>
                {siteConfig.title}
              </Heading>
              <p className={styles.tagline}>{siteConfig.tagline}</p>
              <p className={styles.subtitle}>
                Drop an <code>index.js</code> onto your Pico, open the REPL, and
                iterate fast. Think of it as Node for tiny boards: familiar
                JavaScript, instant feedback, and a modular runtime under the hood.
              </p>
              <div className={styles.actions}>
                <Link className={styles.primaryAction} to="/docs/overview">
                  Read the docs
                </Link>
                <Link className={styles.secondaryAction} to="/docs/quick-start">
                  Quick start
                </Link>
                <Link className={styles.tertiaryAction} to="/docs/glossary">
                  Glossary
                </Link>
              </div>
              <div className={styles.heroMeta}>
                <span>Drag-and-drop scripts</span>
                <span>REPL over USB</span>
                <span>RP2040 + RP2350</span>
              </div>
            </div>
              <div className={styles.heroPanel}>
                <div className={styles.panelHeader}>
                  <div
                    className={styles.chippyWrap}
                    onMouseEnter={() => {
                      cancelHide();
                      setShowChippy(true);
                    }}
                    onMouseLeave={() => {
                      scheduleHide();
                    }}
                  >
                    <Link
                      className={styles.panelButton}
                      to="/docs/meet-chippy"
                      aria-label="Meet Chippy"
                    >
                      <img
                        src={useBaseUrl('/img/chippy-icon.png')}
                        alt="Chippy, the mcujs mascot"
                        className={styles.panelIcon}
                      />
                    </Link>
                    {showChippy ? (
                      <div
                        className={styles.chippyBubble}
                        onMouseEnter={cancelHide}
                        onMouseLeave={scheduleHide}
                      >
                        <div className={styles.chippyText}>Hi! I’m Chippy.</div>
                        <Link className={styles.chippyLink} to="/docs/meet-chippy">
                          Meet Chippy
                        </Link>
                      </div>
                    ) : null}
                  </div>
                  <span>mcujs session</span>
                </div>
                <div className={styles.panelBody}>
                  <div className={styles.panelLine}>mcujs v0.1.0 on pico</div>
                  <div className={styles.panelLine}>&gt; GPIO.init(25, GPIO.OUTPUT)</div>
                  <div className={styles.panelLine}>&gt; setInterval(() =&gt; GPIO.toggle(25), 500)</div>
                  <div className={styles.panelLine}>&gt; console.log('Blinking!')</div>
                  <div className={styles.panelLine}>Blinking!</div>
                </div>
              </div>

          </div>
        </section>
        <section className={styles.sections}>
          <div className={styles.sectionGrid}>
            <div className={styles.card}>
              <Heading as="h2">Quick start workflow</Heading>
              <p>
                Flash a UF2, mount the drive, add <code>index.js</code>, and
                reset. No toolchain needed for most users.
              </p>
              <Link className={styles.cardLink} to="/docs/quick-start">
                Start in minutes
              </Link>
            </div>
            <div className={styles.card}>
              <Heading as="h2">Runtime basics</Heading>
              <p>
                Learn the REPL commands, module loading, and filesystem behavior
                that make iteration quick.
              </p>
              <Link className={styles.cardLink} to="/docs/runtime-basics">
                Explore the runtime
              </Link>
            </div>
            <div className={styles.card}>
              <Heading as="h2">Hardware APIs</Heading>
              <p>
                GPIO, PWM, I2C, SPI, and ADC are available as built-in modules.
                Expand support as new boards come online.
              </p>
              <Link className={styles.cardLink} to="/docs/api-reference">
                Browse APIs
              </Link>
            </div>
          </div>
        </section>
      </main>
    </Layout>
  );
}
