# References

| # | Point | Source |
|---|---|---|
| 1 | C over MicroPython for audio | MicroPython speed docs |
| 2 | Mic can run off a GPIO pin | INMP441 datasheet |
| 3 | Speech-to-text choice | OpenAI model page |
| 4 | Braking threshold must be calibrated | SWOV braking study |
| 5 | Break reminder | Highway Code rule 91 |
| 6 | Data retention design | ICO data protection principles |
| 7 | Privacy framework | LINDDUN (ENISA paper) |
| 8 | Security framework | STRIDE (Shostack book) |

## Details

| # | What it proves | Reference (Harvard) |
|---|---|---|
| 1 | MicroPython needs special non-standard modes to get close to compiled speed, so C was chosen for real-time audio | MicroPython (n.d.) *Maximising MicroPython speed*. Available at: https://docs.micropython.org/en/latest/reference/speed_python.html (Accessed: 6 November 2026). |
| 2 | The mic draws ~1.4 mA and accepts 1.62–3.63 V, so a GPIO pin can power it | TDK InvenSense (n.d.) *INMP441 datasheet*. Available at: https://www.digikey.cn/htmldatasheets/production/1431884/0/0/1/inmp441-datasheet.html (Accessed: 6 November 2026). |
| 3 | The chosen speech-to-text model beats original Whisper on word error rate | OpenAI (n.d.) *GPT-4o mini Transcribe*. Available at: https://developers.openai.com/api/docs/models/gpt-4o-mini-transcribe (Accessed: 6 November 2026). |
| 4 | There is no standard harsh-braking threshold, so it has to be calibrated (the study itself starts at 3.0 m/s²) | SWOV (n.d.) *UDRIVE truck driver harsh braking study*. Available at: https://www.swov.nl/nl/node/17223 (Accessed: 6 November 2026). |
| 5 | Break reminder: 15 minutes every 2 hours is recommended | UK Government (n.d.) *The Highway Code: rules 89 to 102*. Available at: https://www.gov.uk/highway-code/rules-for-drivers-and-motorcyclists-89-to-102 (Accessed: 6 November 2026). |
| 6 | Retention tiers follow the data minimisation and storage limitation principles | Information Commissioner's Office (n.d.) *A guide to the data protection principles*. Available at: https://ico.org.uk/for-organisations/uk-gdpr-guidance-and-resources/data-protection-principles/a-guide-to-the-data-protection-principles/ (Accessed: 6 November 2026). |
| 7 | LINDDUN as the privacy threat framework | Danezis, G. et al. (2015) *Privacy and Data Protection by Design: from policy to engineering*. ENISA. Available at: https://arxiv.org/abs/1501.03726 (Accessed: 6 November 2026). |
| 8 | STRIDE as the security threat framework | Shostack, A. (2014) *Threat Modeling: Designing for Security*. Indianapolis: Wiley. |
